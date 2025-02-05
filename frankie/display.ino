
void show_display(String text1, bool filled) {
  show_display(1, text1, "", filled);
}

void show_display(String text1, String text2, bool filled) {
  show_display(2, text1, text2, filled);
}

void show_display(int lines, String text1, String text2, bool filled) {
  display.clear();
  if (filled) {
    display.drawXbm(0, 16, 128, 64, epd_bitmap_mouse_filled);
    display.setColor(BLACK);
  } else {
    display.drawXbm(0, 16, 128, 64, epd_bitmap_mouse);
  }

  if (lines == 1) {
    display.drawStringMaxWidth(90, 29, 72, text1);
  } else {
    display.drawStringMaxWidth(90, 22, 72, text1);
    display.drawStringMaxWidth(90, 36, 72, text2);
  }

  if (filled)
    display.setColor(WHITE);

  for (int i = 0; i < 3; i++) {
    int pos_x = 128 - 3 * 18 + i * 18;
    if (currentStatus == STATUS_DONE || i < currentAlarm) {
      // Draw checked
      display.drawXbm(pos_x, 1, 14, 14, epd_checked);
    } else {
      // Draw unchecked
      display.drawRect(pos_x + 2, 2, 12, 12);
    }
  }

  display.display();

}
