//---------------------------------------------------------
//   updateHScrollbar
//---------------------------------------------------------

void Editor::updateHScrollbar() {
      int visible = editWidget()->visibleSize().width();
      int maxCol  = 0;
      for (int i = 0; i < kontext()->file()->rows(); ++i) {
            int c = kontext()->file()->columns(i);
            if (c > maxCol)
                  maxCol = c;
            }
      hScroll->blockSignals(true);
      int total = maxCol;
      hScroll->setPageStep(visible);
      hScroll->setRange(0, std::max(0, total));
      hScroll->setValue(kontext()->screenColumnOffset());
      hScroll->blockSignals(false);
      }
