
void show_display(String text1, bool filled) {
  show_display(1, text1, "", filled);
}

void show_display(String text1, String text2, bool filled) {
  show_display(2, text1, text2, filled);
}

void show_display(int lines, String text1, String text2, bool filled) {
  if (debug) {
    debug_display();
    return;
  }
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

  display.drawStringMaxWidth(24, 2, 72, currentTimeFormatted());

  display.display();
}


void debug_display() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 2,  "Time: " + currentTimeFormatted() + ", " + String(currentTimeInMinutes()));
  display.drawString(0, 16, "Alarme: " + currentAlarmFormatted() + ", " + String(currentAlarmInMinutes()));
  display.drawString(0, 25, "Check: " + formatMinutes(lastCheck) + ", " + String(lastCheck));
  display.drawString(0, 34, "Current: " + String(currentAlarm));
  display.drawString(0, 43, "Alert: " + formatMinutes(lastAlert/60) + ", " + String(lastAlert/60));
  display.drawString(0, 52, "Status: " + String(currentStatus == STATUS_OK ? "OK":(currentStatus == STATUS_LATE ? "LATE": "DONE")));
  display.display();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
}