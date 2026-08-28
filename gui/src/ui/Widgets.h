// Reusable widgets: list rows with selection, toggle rows, slider rows,
// section headers. Pure rendering on top of Renderer; no state beyond what
// the caller provides.
#pragma once

#include <string>
#include <vector>

#include "ui/Input.h"
#include "ui/Renderer.h"

namespace n64ui {

// A simple vertical list: rows of text, one selected, scroll offset.
// Each row may carry a right-aligned value column (settings rows).
class ListView {
 public:
  void setRows(const std::vector<std::string>& rows);
  void setRows(const std::vector<std::string>& labels,
               const std::vector<std::string>& values);
  const std::vector<std::string>& rows() const { return m_rows; }
  int selection() const { return m_sel; }
  void move(int delta);
  void draw(Renderer& r, int x, int y, int w, int h, Rgba fg, Rgba selBg,
            Rgba selFg) const;

 private:
  std::vector<std::string> m_rows;
  std::vector<std::string> m_values;
  int m_sel = 0;
  mutable int m_scroll = 0;
};

// Draw a row with label left, value right; used by settings screens.
void drawValueRow(Renderer& r, int x, int y, int w, const std::string& label,
                  const std::string& value, bool selected, Rgba fg, Rgba selBg,
                  Rgba selFg);

// Draw a header bar + optional footer hint line.
void drawScreenChrome(Renderer& r, int w, int h, const std::string& title,
                      const std::string& footer);

}  // namespace n64ui
