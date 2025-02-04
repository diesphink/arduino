
void show_display(String text1, bool filled) {
  show_display(1, text1, "", filled);
}

void show_display(String text1, String text2, bool filled) {
  show_display(2, text1, text2, filled);
}

void show_display(int lines, String text1, String text2, bool filled) {
  display.clear();
  if (filled) {
    display.drawXbm(0, 0, 128, 64, epd_bitmap_mouse_filled);
    display.setColor(BLACK);
  } else {
    display.drawXbm(0, 0, 128, 64, epd_bitmap_mouse);
  }

  if (lines == 1) {
    display.drawStringMaxWidth(90, 29, 72, text1);
  } else {
    display.drawStringMaxWidth(90, 22, 72, text1);
    display.drawStringMaxWidth(90, 36, 72, text2);
  }

  if (filled)
    display.setColor(WHITE);

  for (int i = 0; i < currentAlarm + (currentStatus == STATUS_DONE? 1 : 0); i++) {
    display.fillRect(62 + i * 23, 5, 6, 6);
  }

  display.display();

}
