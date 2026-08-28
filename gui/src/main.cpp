// n64ui - TrimUI Brick mupen64plus frontend.
//
// Usage:
//   n64ui                 launcher (ROM browser)
//   n64ui <rom>           launch a game directly (Menu = in-game menu)
//
// Lifecycle:
//   launcher screen <-> game running (Menu opens the in-game menu while
//   a game runs; the core runs on its own thread).
#include <cstdio>
#include <cstring>
#include <csignal>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include <dirent.h>
#include <sys/stat.h>

#include "core/Emulator.h"
#include "core/Version.h"
#include "core/VidExt.h"
#include "screens/GameMenu.h"
#include "screens/Launcher.h"
#include "ui/Input.h"
#include "ui/Renderer.h"
#include "ui/Screen.h"
#include "util/Log.h"
#include "util/Platform.h"
#include "util/Str.h"

namespace {

volatile sig_atomic_t g_quitRequested = 0;

void onSignal(int) { g_quitRequested = 1; }

const char* kUsage =
    "n64ui - TrimUI Brick N64 frontend\n"
    "usage: n64ui [options] <rom>\n"
    "  <rom>             launch the game directly; MENU = in-game menu\n"
    "  --video <name>    pick the video plugin by name (default glide64mk2,\n"
    "                    e.g. --video rice)\n"
    "  --debug           verbose log\n";

struct App {
  n64ui::Emulator& emu;
  n64ui::Input& input;
  n64ui::Renderer& renderer;
  std::vector<n64ui::Screen*> stack;
  bool menuOpen = false;
  bool running = true;
  int autoRequest = 0;  // N64UI_AUTOTEST: 1=open menu, 2=close, 3=exit
};

void push(App& app, n64ui::Screen* s) {
  s->onShow();
  app.stack.push_back(s);
}

void pop(App& app) {
  if (app.stack.empty()) return;
  app.stack.back()->onHide();
  delete app.stack.back();
  app.stack.pop_back();
  if (!app.stack.empty()) app.stack.back()->onShow();
}

}  // namespace

int main(int argc, char** argv) {
  std::string romArg;
  std::string gfxPrefer;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("%s", kUsage);
      return 0;
    }
    if (strcmp(argv[i], "--version") == 0) {
      printf("n64ui 0x%06X (core api 0x%06X)\n", (unsigned)n64ui::kFrontendVersion,
             (unsigned)n64ui::kCoreApiVersion);
      return 0;
    }
    if (strcmp(argv[i], "--video") == 0 || strcmp(argv[i], "--gfx") == 0) {
      if (i + 1 < argc) gfxPrefer = argv[++i];
      continue;
    }
    if (romArg.empty()) romArg = argv[i];
  }

  // Paths follow the stock launch flow (see mupen64plus_standalone.sh):
  // XDG dirs are inherited from launch.sh (device) or resolved at runtime.
  // Default level is INFO (kills the plugin's verbose chatter); pass
  // --debug for the full log.
  for (int i = 1; i < argc; ++i)
    if (strcmp(argv[i], "--debug") == 0)
      n64ui::logSetLevel(n64ui::LogLevel::Debug);
  LOG_INFO("n64ui starting (api 0x%06X) %s", (unsigned)n64ui::kFrontendVersion,
           n64ui::Platform::isDevice() ? "[device]" : "[host]");

  n64ui::Emulator* emu = n64ui::Emulator::create();
  n64ui::Input* input = n64ui::Input::create();
  n64ui::Renderer* renderer = n64ui::Renderer::create();

  n64ui::EmulatorConfig ecfg;
  ecfg.libPath = n64ui::Platform::emulatorLibPath();
  ecfg.pluginDir = n64ui::Platform::pluginDir();
  ecfg.configDir = n64ui::Platform::configDir();
  ecfg.dataDir = n64ui::Platform::dataDir();
  ecfg.screenWidth = n64ui::Platform::screenWidth();
  ecfg.screenHeight = n64ui::Platform::screenHeight();
  bool emuOk = emu->init(ecfg);
  if (!emuOk) LOG_ERROR("emulator init failed; UI-only mode");
  if (!gfxPrefer.empty()) {
    emu->setGfxPreference(gfxPrefer);
    LOG_INFO("gfx plugin preference: %s", gfxPrefer.c_str());
  }
  // Video-plugin/context compatibility: client-array plugins (Rice) and the
  // GLES backend (device) need the renderer to unbind GL_ARRAY_BUFFER and
  // reset 3D state; the desktop glide64mk2 path (host default) is preserved.
  renderer->setClientArrayCompat(
      n64ui::Platform::isDevice() ||
      (!gfxPrefer.empty() && gfxPrefer.find("rice") != std::string::npos));

  // The renderer's GL objects are created lazily at the first SetVideoMode
  // (when the VidExt creates the context, mirroring the stock core); the
  // menu is the only UI and only draws while a game is paused.
  bool uiActive = true;
  // Hand the renderer to VidExt so its swap path (emulation thread) can paint
  // the in-game menu over the frozen frame.
  if (emuOk) emu->vidext().setRenderer(renderer);

  if (!input->init()) return 1;

  App app{*emu, *input, *renderer, {}, false, true};
  // The swap path (emulation thread) draws the in-game menu: give it a drawer
  // that renders the topmost screen (the GameMenu) over the frozen frame. Only
  // active while a game is paused with the menu open.
  n64ui::VidExt::MenuDrawer menuDrawer = [&app](n64ui::Renderer& r) {
    if (!app.stack.empty()) app.stack.back()->draw(r);
  };
  if (emuOk) emu->vidext().setMenuDrawer(menuDrawer);

  // Autonomous test: drive the menu from the swap path and quit (N64UI_AUTOTEST).
  if (emuOk && getenv("N64UI_AUTOTEST")) {
    app.autoRequest = 0;
    emu->vidext().setAutoCallback([&app](int step) {
      if (step >= 1 && step <= 3) app.autoRequest = step;
    });
  }

  if (!romArg.empty() && emuOk) {
    // Drop our main-thread hold on the context BEFORE the game launches; the
    // game's first SetMode (emulation thread) takes ownership from here.
    emu->vidext().releaseCurrent();
    // Launch directly; Menu opens the in-game menu.
    emu->launch(romArg);
  } else {
    n64ui::Launcher* launcher = new n64ui::Launcher(*emu, n64ui::Platform::romDir());
    push(app, launcher);
  }

  // Ctrl+C / SIGTERM from the console must kill the whole process (windows
  // included), not just stop the game.
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);

  while (app.running && !g_quitRequested) {
    input->waitEvent(100);
    bool uiChanged = false;
    // N64UI_AUTOTEST: act on the swap-path's menu requests.
    if (app.autoRequest == 1) {
      if (emuOk) emu->vidext().setMenuVisible(true);
      push(app, new n64ui::GameMenu(*emu, *input));
      input->resync();
      app.autoRequest = 0;
      LOG_INFO("autotest: menu opened");
    } else if (app.autoRequest == 2) {
      pop(app);
      input->resync();
      if (emuOk) emu->vidext().setMenuVisible(false);
      app.autoRequest = 0;
      LOG_INFO("autotest: menu closed");
    } else if (app.autoRequest == 3) {
      app.autoRequest = 0;
      app.running = false;
    } else if (app.autoRequest == 4 || app.autoRequest == 5) {
      // Simulate a Down navigation press while the menu is open.
      if (!app.stack.empty()) {
        n64ui::Action d;
        d.type = n64ui::ActionType::Down;
        app.stack.back()->handleAction(d);
        if (emuOk) emu->vidext().menuChanged();
      }
      app.autoRequest = 0;
      LOG_INFO("autotest: navigated down");
    }
    for (const n64ui::Action& a : input->pollActions()) {
      // Menu (Escape) with no menu open: open the in-game menu. With the menu
      // open, Escape behaves like B (one level back) via handleAction below.
      if (a.type == n64ui::ActionType::Menu && !romArg.empty() &&
          app.stack.empty()) {
        if (emuOk) emu->vidext().setMenuVisible(true);
        // onShow -> pause; drawn by the swap path once at pause entry.
        push(app, new n64ui::GameMenu(*emu, *input));
        if (emuOk) emu->vidext().menuChanged();
        input->resync();
        uiChanged = true;
        continue;
      }
      if (a.type == n64ui::ActionType::Power) {
        app.running = false;
        continue;
      }
      if (!app.stack.empty()) {
        n64ui::ScreenResult r = app.stack.back()->handleAction(a);
        uiChanged = true;
        if (emuOk) emu->vidext().menuChanged();
        if (r.kind == n64ui::ScreenResult::Pop) {
          pop(app);  // onHide (GameMenu) waits for key release + resumes
          input->resync();
          // The menu is gone only when the stack emptied (root GameMenu
          // popped). Sub-menu pops keep the menu open and visible.
          if (!romArg.empty() && app.stack.empty()) {
            if (emuOk) emu->vidext().setMenuVisible(false);
          } else if (romArg.empty() && app.stack.empty()) {
            app.running = false;
          }
        } else if (r.kind == n64ui::ScreenResult::Push) {
          push(app, r.next);
          input->resync();
        }
      } else if (a.type == n64ui::ActionType::B) {
        // No launcher anymore: with no menu open this is just the game's B
        // button. Only quit from Power or the Exit menu item.
      }
    }
    // Raw-input capture (input mapping screen): poll the SDL queue each
    // iteration (never block) so the menu keeps repainting with the
    // "Press a key..." overlay; fall through to the repaint below.
    if (!app.stack.empty() && app.stack.back()->wantsRawCapture()) {
      std::string binding;
      bool ok = input->capturePoll(binding);
      if (!ok) {
        app.stack.back()->submitRawCapture("", true);  // cancelled
        uiChanged = true;
      } else if (!binding.empty()) {
        app.stack.back()->submitRawCapture(binding, false);
        uiChanged = true;
      } else {
        app.stack.back()->captureTick();
      }
    }
    // When a game launched directly ends (Exit / stop), quit the whole app:
    // this is an in-game-menu-only frontend, so there is no launcher screen.
    // Only fire once the game actually started (state is STOPPED briefly
    // between launch and the core's first RUNNING notification).
    if (!romArg.empty() && app.stack.empty() && emu->gameStarted() &&
        emu->state() == M64EMU_STOPPED && emuOk) {
      app.running = false;
      continue;
    }
    // While the in-game menu is open, repaint it by advancing one frame on
    // the emulation thread: the paused core runs one frame (which the opaque
    // menu hides), the swap path repaints the menu, and the core re-pauses
    // automatically. No context handoffs between threads.
    if (!romArg.empty() && !app.stack.empty() && uiChanged && emuOk) {
      emu->frameAdvance();
    }
    // Draw the launcher ONLY when no game is running: during a game the
    // context belongs to the emulation thread.
    if (uiActive && romArg.empty() && !app.stack.empty()) {
      n64ui::VidExt& v = emu->vidext();
      for (int i = 0; i < 100 && !v.makeCurrent(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      renderer->beginFrame(true);
      for (n64ui::Screen* s : app.stack) s->draw(*renderer);
      renderer->endFrame();
      v.swap();
      v.releaseCurrent();
    }
  }

  pop(app);  // unwinds the stack (resumes/cleans up)
  emu->shutdown();
  input->shutdown();
  delete renderer;
  delete input;
  delete emu;
  LOG_INFO("n64ui exiting");
  return 0;
}
