#!/usr/bin/env python3
"""Guard Murphy M4 settings controls against out-of-canvas geometry and stale hit boxes."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "main/apps/app_device_settings.c").read_text()


def require(pattern: str, message: str) -> None:
    if not re.search(pattern, SOURCE, re.S):
        raise AssertionError(message)


def inside(rect: tuple[int, int, int, int], canvas: tuple[int, int] = (480, 736)) -> bool:
    x, y, width, height = rect
    return x >= 0 and y >= 0 and width > 0 and height > 0 and x + width <= canvas[0] and y + height <= canvas[1]


assert all(inside((24, 208 + i * 96, 432, 76)) for i in range(4))
assert inside((40, 392, 400, 62))
assert inside((24, 188, 432, 150))
assert inside((24, 366, 432, 64)) and inside((24, 454, 432, 64))
assert inside((32, 346, 416, 54)) and inside((32, 526, 416, 58))
assert inside((24, 302, 432, 58)) and inside((24, 382, 432, 58))
assert inside((24, 208, 432, 104)) and inside((24, 326, 432, 104))

require(r"EpdRect box = settings_choice_rect\(i\)",
        "M4 auto-lock options must paint through shared geometry helpers")
require(r"ui_rect_hit\(settings_choice_rect\(i\)",
        "M4 auto-lock hit testing must use the painted rectangles")
require(r"ui_draw_button\(fb,\s*settings_boot_button\(\)",
        "M4 BOOT button must paint through the geometry helper")
require(r"ui_rect_hit\(settings_boot_button\(\)",
        "M4 BOOT hit testing must use the painted rectangle")
require(r"ui_rect_hit\(settings_config_button\(0\).*?ui_rect_hit\(settings_config_button\(1\)",
        "Configuration save/restore hit boxes must match their buttons")
require(r"ui_rect_hit\(settings_upgrade_action\(false\).*?ui_rect_hit\(settings_upgrade_action\(true\)",
        "Upgrade hit boxes must match OTA and SD buttons")
require(r"ui_rect_hit\(settings_pin_action\(false\).*?ui_rect_hit\(settings_pin_action\(true\)",
        "PIN hit boxes must match the visible actions")

print("PASS: M4 settings controls fit the content canvas and paint/hit geometry is shared")
