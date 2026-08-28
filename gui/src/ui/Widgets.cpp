// WidgetsImpl: adaptive text rows (row height derives from the current font
// size so screens can switch to a large font for small screens).
#include "ui/Widgets.h"

#include "util/Log.h"

namespace n64ui {

namespace {
constexpr int kRowPad = 8;   // vertical padding around each row's text
constexpr int kPadX = 24;
constexpr int kHeaderPad = 12;
constexpr int kFooterPad = 12;
}  // namespace

void ListView::setRows(const std::vector<std::string>& rows) {
  setRows(rows, std::vector<std::string>(rows.size()));
}

void ListView::setRows(const std::vector<std::string>& labels,
                       const std::vector<std::string>& values) {
  m_rows = labels;
  m_values = values;
  if (m_values.size() != m_rows.size()) m_values.assign(m_rows.size(), "");
  if (m_sel >= (int)m_rows.size()) m_sel = 0;
  m_scroll = 0;
}

void ListView::move(int delta) {
  if (m_rows.empty()) return;
  m_sel = (m_sel + delta + (int)m_rows.size()) % (int)m_rows.size();
  if (m_sel < m_scroll) m_scroll = m_sel;
}

void ListView::draw(Renderer& r, int x, int y, int w, int h, Rgba fg, Rgba selBg,
                    Rgba selFg) const {
  int rowH = r.textHeight() + kRowPad;
  int visible = h / rowH;
  if (m_sel >= m_scroll + visible) m_scroll = m_sel - visible + 1;
  for (int i = 0; i < visible; ++i) {
    int idx = m_scroll + i;
    if (idx >= (int)m_rows.size()) break;
    int ry = y + i * rowH;
    bool sel = idx == m_sel;
    drawValueRow(r, x, ry, w, m_rows[idx],
                 idx < (int)m_values.size() ? m_values[idx] : "", sel, fg,
                 selBg, selFg);
  }
}

void drawValueRow(Renderer& r, int x, int y, int w, const std::string& label,
                  const std::string& value, bool selected, Rgba fg, Rgba selBg,
                  Rgba selFg) {
  int rowH = r.textHeight() + 8;
  int textH = r.textHeight();
  Rgba color = selected ? selFg : fg;
  if (selected) r.drawRect(x, y, w, rowH, selBg);
  r.drawText(x + kPadX, y + (rowH - textH) / 2, label, color);
  if (!value.empty()) {
    int vx = x + w - kPadX - r.textWidth(value);
    r.drawText(vx, y + (rowH - textH) / 2, value, color);
  }
}

void drawScreenChrome(Renderer& r, int w, int h, const std::string& title,
                      const std::string& footer) {
  r.drawRectOutline(0, 0, w, h, Rgba::rgb(90, 90, 90));
  if (!title.empty()) {
    r.drawText(kHeaderPad, kHeaderPad, title, Rgba::rgb(255, 255, 136));
  }
  if (!footer.empty()) {
    int fw = r.textWidth(footer);
    r.drawText(w - kFooterPad - fw, h - kFooterPad - r.textHeight(), footer,
               Rgba::rgb(160, 160, 160));
  }
}

}  // namespace n64ui
