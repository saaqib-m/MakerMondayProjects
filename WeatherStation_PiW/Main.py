"""
Wireless Weather Display v2
Pico W + SSD1306 OLED + rotary encoder
Cycle through screens: Current / Hourly / 7-Day / Sunrise-Sunset
Pixel-art weather icons, live clock in corner, no sensors.
"""

import network
import urequests
import ntptime
import time
from machine import Pin, I2C, PWM
import ssd1306

# ---------------- CONFIG ----------------
WIFI_SSID = "YOUR_WIFI_NAME"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"

CITY_NAME = "London"
LATITUDE = 51.5072
LONGITUDE = -0.1276
UTC_OFFSET_HOURS = 0        # e.g. -5 for US Eastern, 1 for UK summer time

REFRESH_SECONDS = 600        # re-fetch weather every 10 min

# RGB LED (common cathode) — temperature color indicator
LED_R_PIN = 6
LED_G_PIN = 7
LED_B_PIN = 8
COMMON_ANODE = False         # set True if using a common anode LED
TEMP_MIN = -10                # gradient low end (deg C) -> blue
TEMP_MAX = 35                 # gradient high end (deg C) -> red
# -----------------------------------------

i2c = I2C(0, scl=Pin(1), sda=Pin(0), freq=400000)
oled = ssd1306.SSD1306_I2C(128, 64, i2c)


# ---------------- LOW-LEVEL DRAWING (pixel-only, for minimal drivers) ----------------

def px(x, y, c=1):
    if 0 <= x < 128 and 0 <= y < 64:
        oled.pixel(x, y, c)


def draw_line(x0, y0, x1, y1, c=1):
    x0, y0, x1, y1 = int(x0), int(y0), int(x1), int(y1)
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        px(x0, y0, c)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def draw_rect_filled(x, y, w, h, c=1):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            px(xx, yy, c)

clk = Pin(2, Pin.IN, Pin.PULL_UP)
dt = Pin(3, Pin.IN, Pin.PULL_UP)
sw = Pin(4, Pin.IN, Pin.PULL_UP)

pwm_r = PWM(Pin(LED_R_PIN))
pwm_g = PWM(Pin(LED_G_PIN))
pwm_b = PWM(Pin(LED_B_PIN))
for _pwm in (pwm_r, pwm_g, pwm_b):
    _pwm.freq(1000)

SCREEN_NAMES = ["current", "hourly", "daily", "sun"]
current_screen = 0
last_clk_state = clk.value()
manual_refresh = False
last_button_time = 0
last_encoder_time = 0

weather_cache = None
last_fetch_time = 0

WEATHER_TEXT = {
    0: "Clear", 1: "Mostly clear", 2: "Partly cloudy", 3: "Overcast",
    45: "Fog", 48: "Fog",
    51: "Lt drizzle", 53: "Drizzle", 55: "Heavy drizzle",
    61: "Lt rain", 63: "Rain", 65: "Heavy rain",
    71: "Lt snow", 73: "Snow", 75: "Heavy snow",
    80: "Showers", 81: "Showers", 82: "Violent showers",
    95: "Thunderstorm",
}

# Short forms (<=6 chars) for tight columns like the daily forecast list —
# clean abbreviations instead of mid-word truncation
WEATHER_SHORT = {
    0: "Clear", 1: "Clear", 2: "Cloudy", 3: "Ovrcst",
    45: "Fog", 48: "Fog",
    51: "Drzl", 53: "Drzl", 55: "Drzl",
    61: "Rain", 63: "Rain", 65: "Rain",
    71: "Snow", 73: "Snow", 75: "Snow",
    80: "Rain", 81: "Rain", 82: "Rain",
    95: "Storm",
}


# ---------------- PIXEL ART ICONS ----------------

def draw_circle(cx, cy, r):
    for dy in range(-r, r + 1):
        span = int((r * r - dy * dy) ** 0.5)
        draw_rect_filled(cx - span, cy + dy, 2 * span + 1, 1, 1)


def icon_cloud(x, y, s=1):
    draw_circle(x + 5 * s, y + 7 * s, 3 * s)
    draw_circle(x + 9 * s, y + 5 * s, 4 * s)
    draw_circle(x + 13 * s, y + 7 * s, 3 * s)
    draw_rect_filled(x + 3 * s, y + 7 * s, 11 * s, 4 * s, 1)


def icon_sun(x, y, s=1):
    cx, cy = x + 8 * s, y + 8 * s
    r = 4 * s
    draw_circle(cx, cy, r)
    draw_line(cx, y, cx, y + 2 * s, 1)
    draw_line(cx, y + 13 * s, cx, y + 15 * s, 1)
    draw_line(x, cy, x + 2 * s, cy, 1)
    draw_line(x + 13 * s, cy, x + 15 * s, cy, 1)
    draw_line(x + 2 * s, y + 2 * s, x + 4 * s, y + 4 * s, 1)
    draw_line(x + 11 * s, y + 2 * s, x + 13 * s, y + 4 * s, 1)
    draw_line(x + 2 * s, y + 13 * s, x + 4 * s, y + 11 * s, 1)
    draw_line(x + 11 * s, y + 13 * s, x + 13 * s, y + 11 * s, 1)


def icon_partly_cloudy(x, y, s=1):
    draw_circle(x + 5 * s, y + 4 * s, int(2.5 * s))
    icon_cloud(x + 1 * s, y + 3 * s, s)


def icon_rain(x, y, s=1):
    icon_cloud(x, y - 2 * s, s)
    for dx in (2, 6, 10):
        draw_line(x + dx * s, y + 12 * s, x + (dx - 1) * s, y + 15 * s, 1)


def icon_snow(x, y, s=1):
    icon_cloud(x, y - 2 * s, s)
    for dx in (2, 6, 10):
        cx, cy = x + dx * s, y + 13 * s
        oled.pixel(cx, cy, 1)
        draw_line(cx - 1, cy, cx + 1, cy, 1)
        draw_line(cx, cy - 1, cx, cy + 1, 1)


def icon_storm(x, y, s=1):
    icon_cloud(x, y - 2 * s, s)
    draw_line(x + 8 * s, y + 11 * s, x + 5 * s, y + 14 * s, 1)
    draw_line(x + 5 * s, y + 14 * s, x + 8 * s, y + 14 * s, 1)
    draw_line(x + 8 * s, y + 14 * s, x + 5 * s, y + 17 * s if y + 17 * s < 64 else 63, 1)


def icon_fog(x, y, s=1):
    for row in range(4):
        draw_rect_filled(x + 1 * s, y + (4 + row * 3) * s, 14 * s, 1, 1)


def get_icon_func(code):
    if code == 0:
        return icon_sun
    if code in (1,):
        return icon_sun
    if code in (2,):
        return icon_partly_cloudy
    if code == 3:
        return icon_cloud
    if code in (45, 48):
        return icon_fog
    if code in (51, 53, 55, 61, 63, 65, 80, 81, 82):
        return icon_rain
    if code in (71, 73, 75):
        return icon_snow
    if code == 95:
        return icon_storm
    return icon_cloud


# ---------------- RGB TEMPERATURE LED ----------------

# Gradient stops: cold -> hot
_GRADIENT = [
    (0, 0, 255),    # blue    (coldest)
    (0, 255, 255),  # cyan
    (0, 255, 0),    # green
    (255, 255, 0),  # yellow
    (255, 0, 0),    # red     (hottest)
]


def temp_to_color(temp_c):
    t = (temp_c - TEMP_MIN) / (TEMP_MAX - TEMP_MIN)
    t = max(0.0, min(1.0, t))  # clamp 0..1

    segments = len(_GRADIENT) - 1
    scaled = t * segments
    idx = int(scaled)
    if idx >= segments:
        idx = segments - 1
    frac = scaled - idx

    r1, g1, b1 = _GRADIENT[idx]
    r2, g2, b2 = _GRADIENT[idx + 1]
    r = int(r1 + (r2 - r1) * frac)
    g = int(g1 + (g2 - g1) * frac)
    b = int(b1 + (b2 - b1) * frac)
    return r, g, b


def set_led_rgb(r, g, b):
    def to_duty(v):
        v = max(0, min(255, v))
        duty = int(v / 255 * 65535)
        if COMMON_ANODE:
            duty = 65535 - duty
        return duty

    pwm_r.duty_u16(to_duty(r))
    pwm_g.duty_u16(to_duty(g))
    pwm_b.duty_u16(to_duty(b))


def update_led_for_temp(temp_c):
    r, g, b = temp_to_color(temp_c)
    set_led_rgb(r, g, b)


# ---------------- SETUP ----------------

def connect_wifi():
    oled.fill(0)
    oled.text("Connecting WiFi", 0, 0)
    oled.show()
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    timeout = 15
    while not wlan.isconnected() and timeout > 0:
        time.sleep(1)
        timeout -= 1
    if not wlan.isconnected():
        oled.fill(0)
        oled.text("WiFi FAILED", 0, 0)
        oled.show()
        raise RuntimeError("no wifi")
    try:
        ntptime.settime()
    except Exception:
        pass
    oled.fill(0)
    oled.text("Connected!", 0, 0)
    oled.show()
    time.sleep(1)


def fetch_weather():
    url = (
        "https://api.open-meteo.com/v1/forecast"
        f"?latitude={LATITUDE}&longitude={LONGITUDE}"
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
        "&hourly=temperature_2m,weather_code"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
        "&timezone=auto&forecast_days=7"
    )
    r = urequests.get(url)
    data = r.json()
    r.close()
    return data


# ---------------- SCREEN RENDERERS ----------------

def draw_clock():
    now = time.localtime(time.time() + UTC_OFFSET_HOURS * 3600)
    txt = "{:02d}:{:02d}".format(now[3], now[4])
    oled.text(txt, 128 - len(txt) * 8, 0)


def render_current(data):
    cur = data["current"]
    temp = cur["temperature_2m"]
    hum = cur["relative_humidity_2m"]
    wind = cur["wind_speed_10m"]
    code = cur["weather_code"]

    oled.fill(0)
    oled.text(CITY_NAME[:10], 0, 0)
    draw_clock()
    get_icon_func(code)(0, 14, 2)          # big 32x32 icon, rows 14-45
    oled.text("{:.0f}C".format(temp), 40, 22)
    oled.text(WEATHER_TEXT.get(code, "?")[:16], 0, 48)       # full-width row, no overlap
    oled.text("H:{}% W:{:.0f}kmh".format(hum, wind)[:16], 0, 56)
    oled.show()


def render_hourly(data):
    hourly = data["hourly"]
    times = hourly["time"]
    temps = hourly["temperature_2m"]
    codes = hourly["weather_code"]

    # find index closest to "now" then take next 4 hours
    now_str = format_now_iso()
    start = 0
    for i, t in enumerate(times):
        if t >= now_str:
            start = i
            break

    oled.fill(0)
    oled.text("Next hours", 0, 0)
    draw_clock()
    for i in range(4):
        idx = start + i
        if idx >= len(times):
            break
        hour_label = times[idx][11:13] + "h"   # e.g. "14h" - short, fits column
        x = i * 32
        oled.text(hour_label, x, 14)
        get_icon_func(codes[idx])(x, 24, 1)
        oled.text("{:.0f}C".format(temps[idx]), x, 42)
    oled.show()


def render_daily(data):
    daily = data["daily"]
    dates = daily["time"]
    codes = daily["weather_code"]
    hi = daily["temperature_2m_max"]
    lo = daily["temperature_2m_min"]
    days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]

    oled.fill(0)
    oled.text("7-Day", 0, 0)
    draw_clock()
    for i in range(min(5, len(dates))):
        y = 12 + i * 10
        # rough weekday from ISO date (YYYY-MM-DD)
        parts = dates[i].split("-")
        wk = day_of_week(int(parts[0]), int(parts[1]), int(parts[2]))
        label = "{} {:.0f}/{:.0f} {}".format(
            days[wk], hi[i], lo[i], WEATHER_SHORT.get(codes[i], "")
        )
        oled.text(label[:16], 0, y)
    oled.show()


def render_sun(data):
    daily = data["daily"]
    sunrise = daily["sunrise"][0][11:16]
    sunset = daily["sunset"][0][11:16]

    oled.fill(0)
    oled.text("Sun today", 0, 0)
    draw_clock()
    # simple arc: horizon line + half circle, sized to leave room for text below
    cx, cy, r = 64, 38, 20
    for angle in range(0, 181, 6):
        import math
        rad = math.radians(angle)
        arc_x = cx - int(r * math.cos(rad))
        arc_y = cy - int(r * math.sin(rad))
        px(arc_x, arc_y, 1)
    draw_rect_filled(cx - r, cy, 2 * r, 1, 1)
    oled.text("Rise " + sunrise, 0, 46)
    oled.text("Set  " + sunset, 0, 56)
    oled.show()


RENDERERS = {
    "current": render_current,
    "hourly": render_hourly,
    "daily": render_daily,
    "sun": render_sun,
}


# ---------------- HELPERS ----------------

def format_now_iso():
    now = time.localtime(time.time() + UTC_OFFSET_HOURS * 3600)
    return "{:04d}-{:02d}-{:02d}T{:02d}:{:02d}".format(now[0], now[1], now[2], now[3], now[4])


def day_of_week(y, m, d):
    # Zeller-ish calc, returns 0=Mon..6=Sun
    t = [0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4]
    if m < 3:
        y -= 1
    w = (y + y // 4 - y // 100 + y // 400 + t[m - 1] + d) % 7
    return (w + 5) % 7  # shift so 0=Mon


# ---------------- ENCODER HANDLERS ----------------

def encoder_callback(pin):
    global last_clk_state, current_screen, last_encoder_time
    now = time.ticks_ms()
    if time.ticks_diff(now, last_encoder_time) < 2:
        return
    last_encoder_time = now
    clk_state = clk.value()
    if clk_state != last_clk_state:
        if dt.value() != clk_state:
            current_screen = (current_screen + 1) % len(SCREEN_NAMES)
        else:
            current_screen = (current_screen - 1) % len(SCREEN_NAMES)
        last_clk_state = clk_state


def button_callback(pin):
    global manual_refresh, last_button_time
    now = time.ticks_ms()
    if time.ticks_diff(now, last_button_time) < 300:
        return
    last_button_time = now
    manual_refresh = True


clk.irq(trigger=Pin.IRQ_FALLING | Pin.IRQ_RISING, handler=encoder_callback)
sw.irq(trigger=Pin.IRQ_FALLING, handler=button_callback)


# ---------------- MAIN LOOP ----------------

def main():
    global weather_cache, last_fetch_time, manual_refresh, current_screen

    connect_wifi()
    weather_cache = fetch_weather()
    last_fetch_time = time.time()
    update_led_for_temp(weather_cache["current"]["temperature_2m"])

    last_drawn_screen = -1

    while True:
        now = time.time()
        if manual_refresh or (now - last_fetch_time) > REFRESH_SECONDS:
            try:
                weather_cache = fetch_weather()
                last_fetch_time = now
                update_led_for_temp(weather_cache["current"]["temperature_2m"])
            except Exception as e:
                oled.fill(0)
                oled.text("Fetch error", 0, 0)
                oled.text(str(e)[:16], 0, 12)
                oled.show()
                time.sleep(2)
            manual_refresh = False

        screen_name = SCREEN_NAMES[current_screen]
        RENDERERS[screen_name](weather_cache)
        last_drawn_screen = current_screen

        # redraw every second so the clock stays live, unless screen changes sooner
        for _ in range(10):
            time.sleep(0.1)
            if current_screen != last_drawn_screen:
                break


main()
