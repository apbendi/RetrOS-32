/**
 * @file bcalc.cpp
 * @brief Basic calculator: add, subtract, multiply and divide decimal numbers.
 *
 * Works like a pocket calculator: each operation is applied to the running
 * result as soon as the next operator or = is pressed, so 2 + 3 x 4 = 20.
 *
 * Buttons can be clicked or typed: digits, '.', + - * / (or x), Enter or =
 * for equals, C or Esc to clear, Backspace to delete the last digit or point,
 * and F9 to change the sign.
 */

#include <utils/Graphics.hpp>
#include <colors.h>

/* Inner window size */
#define WINDOW_WIDTH 184
#define WINDOW_HEIGHT 204
#define MARGIN 6
#define GAP 4

#define DISPLAY_HEIGHT 30
#define BUTTON_WIDTH 40
#define BUTTON_HEIGHT 28
#define GRID_TOP (MARGIN + DISPLAY_HEIGHT + MARGIN)

/* Most digits a number can be typed or shown with; larger results are an error */
#define MAX_DIGITS 12
#define CHAR_SIZE 8
#define FLASH_MS 60
#define KEY_ESCAPE 27
#define KEY_BACKSPACE '\b'
#define KEY_NEGATE KEY_F9   /* same shortcut as Windows Calculator */

/* VGA palette colors */
#define BACKGROUND_COLOR COLOR_VGA_MEDIUM_LIGHT_GRAY
#define DIGIT_FACE_COLOR COLOR_VGA_LIGHTEST_GRAY
#define OPERATOR_FACE_COLOR COLOR_VGA_LIGHTER_GRAY
#define PRESSED_FACE_COLOR COLOR_VGA_LIGHT_GRAY
#define EDGE_LIGHT_COLOR COLOR_WHITE
#define EDGE_DARK_COLOR COLOR_VGA_MEDIUM_DARK_GRAY
#define DISPLAY_COLOR COLOR_WHITE
#define TEXT_COLOR COLOR_BLACK
#define CLEAR_TEXT_COLOR COLOR_VGA_RED
#define INDICATOR_COLOR COLOR_VGA_MEDIUM_GRAY

/* Powers of ten are exact as doubles, so digit math with them stays exact */
static const double powers_of_ten[MAX_DIGITS + 1] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12
};

struct Button {
    int column;
    int row;
    int span;           /* columns the button covers */
    int key;            /* key it stands for, as typed */
    const char* label;
};

/* Drawing and click detection both come from this table, so they always agree */
static const Button buttons[] = {
    {0, 0, 1, KEY_BACKSPACE, "<-"}, {1, 0, 2, 'C', "C"},                  {3, 0, 1, '/', "/"},
    {0, 1, 1, '7', "7"},            {1, 1, 1, '8', "8"}, {2, 1, 1, '9', "9"}, {3, 1, 1, '*', "x"},
    {0, 2, 1, '4', "4"},            {1, 2, 1, '5', "5"}, {2, 2, 1, '6', "6"}, {3, 2, 1, '-', "-"},
    {0, 3, 1, '1', "1"},            {1, 3, 1, '2', "2"}, {2, 3, 1, '3', "3"}, {3, 3, 1, '+', "+"},
    {0, 4, 1, KEY_NEGATE, "-/+"},   {1, 4, 1, '0', "0"}, {2, 4, 1, '.', "."}, {3, 4, 1, '=', "="},
};
#define BUTTON_COUNT ((int)(sizeof(buttons) / sizeof(buttons[0])))

static int buttonX(const Button& button) { return MARGIN + button.column * (BUTTON_WIDTH + GAP); }
static int buttonY(const Button& button) { return GRID_TOP + button.row * (BUTTON_HEIGHT + GAP); }
static int buttonWidth(const Button& button) { return button.span * BUTTON_WIDTH + (button.span - 1) * GAP; }

static bool isDigit(int c) { return c >= '0' && c <= '9'; }
static bool isOperator(int c) { return c == '+' || c == '-' || c == '*' || c == '/'; }

static int textLength(const char* text)
{
    int length = 0;
    while (text[length]) {
        length++;
    }
    return length;
}

static void copyText(char* dest, const char* src)
{
    while ((*dest++ = *src++)) {
    }
}

/* Converts typed digits such as "12.50" to a number */
static double parseEntry(const char* entry)
{
    /* Collect every digit as one whole number, then divide once, so typed values round only once */
    double number = 0;
    int decimals = 0;
    bool afterPoint = false;

    for (int i = 0; entry[i]; i++) {
        if (entry[i] == '.') {
            afterPoint = true;
            continue;
        }
        number = number * 10 + (entry[i] - '0');
        if (afterPoint) {
            decimals++;
        }
    }
    return number / powers_of_ten[decimals];
}

/**
 * Writes value in plain decimal with at most MAX_DIGITS digits, rounding the
 * last one and dropping trailing zeros. Returns false if value is too large.
 * Output needs room for a sign, MAX_DIGITS digits, a point and a terminator.
 */
static bool formatNumber(double value, char* out)
{
    bool negative = value < 0;
    if (negative) {
        value = -value;
    }

    /* Spend the digits the whole part doesn't need on decimals */
    int wholeDigits = 1;
    while (wholeDigits < MAX_DIGITS && value >= powers_of_ten[wholeDigits]) {
        wholeDigits++;
    }
    int decimals = MAX_DIGITS - wholeDigits;

    /* Rounding can carry into an extra whole digit (9.999... becomes 10), so give up a decimal when it does */
    double scaled = value * powers_of_ten[decimals] + 0.5;
    while (scaled >= powers_of_ten[MAX_DIGITS] && decimals > 0) {
        decimals--;
        scaled = value * powers_of_ten[decimals] + 0.5;
    }
    if (scaled >= powers_of_ten[MAX_DIGITS]) {
        return false;
    }

    /* Peel off digits from the most significant; the fraction left at the end is the rounding remainder */
    char digits[MAX_DIGITS];
    bool nonZero = false;
    for (int i = 0; i < MAX_DIGITS; i++) {
        double place = powers_of_ten[MAX_DIGITS - 1 - i];
        int digit = 0;
        while (scaled >= place) {
            scaled -= place;
            digit++;
        }
        digits[i] = '0' + digit;
        nonZero = nonZero || digit != 0;
    }

    int pointAt = MAX_DIGITS - decimals;   /* digits before the point */
    int start = 0;
    while (start < pointAt - 1 && digits[start] == '0') {
        start++;
    }
    int end = MAX_DIGITS;
    while (end > pointAt && digits[end - 1] == '0') {
        end--;
    }

    if (negative && nonZero) {
        *out++ = '-';
    }
    for (int i = start; i < end; i++) {
        if (i == pointAt) {
            *out++ = '.';
        }
        *out++ = digits[i];
    }
    *out = '\0';
    return true;
}

class Calculator : public Window {
public:
    Calculator() : Window(WINDOW_WIDTH, WINDOW_HEIGHT, "bcalc", 0) {
        clearAll();

        drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BACKGROUND_COLOR);
        for (int i = 0; i < BUTTON_COUNT; i++) {
            drawButton(buttons[i], false);
        }
        drawDisplay();
    }

    void click(int x, int y) {
        for (int i = 0; i < BUTTON_COUNT; i++) {
            const Button& button = buttons[i];
            if (x >= buttonX(button) && x < buttonX(button) + buttonWidth(button) &&
                y >= buttonY(button) && y < buttonY(button) + BUTTON_HEIGHT) {
                press(button);
                return;
            }
        }
    }

    void keyPressed(int key) {
        switch (key) {
        case '\n':
            key = '=';
            break;
        case 'x':
        case 'X':
            key = '*';
            break;
        case 'c':
        case KEY_ESCAPE:
            key = 'C';
            break;
        default:
            break;
        }

        const Button* button = findButton(key);
        if (button != nullptr) {
            press(*button);
        }
    }

private:
    char m_entry[MAX_DIGITS + 2];   /* number being typed: digits, a point and a terminator */
    int m_entryLength;
    bool m_entryNegative;           /* sign of the number being typed, kept apart from its digits */
    bool m_entering;                /* display shows m_entry rather than m_result */
    double m_result;                /* running result, the left side of m_pending */
    char m_pending;                 /* operator waiting for its right side, or 0 */
    bool m_error;

    const Button* findButton(int key) {
        for (int i = 0; i < BUTTON_COUNT; i++) {
            if (buttons[i].key == key) {
                return &buttons[i];
            }
        }
        return nullptr;
    }

    /* Shows the button pressed while the result appears, then pops it back up */
    void press(const Button& button) {
        drawButton(button, true);

        int key = button.key;
        if (key == 'C') {
            clearAll();
        } else if (key == KEY_BACKSPACE) {
            backspace();
        } else if (key == KEY_NEGATE) {
            toggleSign();
        } else if (isDigit(key)) {
            enterDigit(key);
        } else if (key == '.') {
            enterPoint();
        } else if (isOperator(key)) {
            applyOperator(key);
        } else if (key == '=') {
            equals();
        }
        drawDisplay();

        sleep(FLASH_MS);
        drawButton(button, false);
    }

    void clearAll() {
        m_entry[0] = '\0';
        m_entryLength = 0;
        m_entryNegative = false;
        m_entering = false;
        m_result = 0;
        m_pending = 0;
        m_error = false;
    }

    void startEntry() {
        m_entry[0] = '\0';
        m_entryLength = 0;
        m_entryNegative = false;
        m_entering = true;
    }

    void append(char c) {
        m_entry[m_entryLength++] = c;
        m_entry[m_entryLength] = '\0';
    }

    int entryDigits() {
        int count = 0;
        for (int i = 0; i < m_entryLength; i++) {
            if (isDigit(m_entry[i])) {
                count++;
            }
        }
        return count;
    }

    bool entryHasPoint() {
        for (int i = 0; i < m_entryLength; i++) {
            if (m_entry[i] == '.') {
                return true;
            }
        }
        return false;
    }

    void enterDigit(char digit) {
        if (m_error) {
            clearAll();
        }
        if (!m_entering) {
            startEntry();
        }

        /* A lone leading zero is replaced rather than kept */
        if (m_entryLength == 1 && m_entry[0] == '0') {
            m_entry[0] = digit;
            return;
        }
        if (entryDigits() < MAX_DIGITS) {
            append(digit);
        }
    }

    void enterPoint() {
        if (m_error) {
            clearAll();
        }
        if (!m_entering) {
            startEntry();
        }
        if (entryHasPoint()) {
            return;
        }

        if (m_entryLength == 0) {
            append('0');
        }
        append('.');
    }

    /* Deletes the last digit or point of the number being typed; results can't be edited */
    void backspace() {
        if (m_error || !m_entering) {
            return;
        }
        if (m_entryLength > 0) {
            m_entry[--m_entryLength] = '\0';
        }

        /* With nothing left, drop the sign too, rather than leave "-0" */
        if (m_entryLength == 0) {
            m_entryNegative = false;
        }
    }

    /* Changes the sign of the number on the display */
    void toggleSign() {
        if (m_error) {
            return;
        }
        if (m_entering) {
            m_entryNegative = !m_entryNegative;
            return;
        }

        /* Right after an operator, the next number to be typed starts out negative */
        if (m_pending != 0) {
            startEntry();
            m_entryNegative = true;
            return;
        }
        m_result = -m_result;
    }

    /* Pressing another operator before typing a number just replaces the pending one */
    void applyOperator(char op) {
        if (m_error) {
            return;
        }
        if (m_entering) {
            commitEntry();
            if (m_error) {
                return;
            }
        }
        m_pending = op;
    }

    void equals() {
        if (m_error) {
            return;
        }
        if (m_entering) {
            commitEntry();
        }
        m_pending = 0;
    }

    /* Folds the typed number into the running result */
    void commitEntry() {
        double value = parseEntry(m_entry);
        if (m_entryNegative) {
            value = -value;
        }
        m_entering = false;

        if (m_pending == 0) {
            m_result = value;
            return;
        }
        calculate(value);
    }

    void calculate(double operand) {
        double result = 0;
        switch (m_pending) {
        case '+':
            result = m_result + operand;
            break;
        case '-':
            result = m_result - operand;
            break;
        case '*':
            result = m_result * operand;
            break;
        case '/':
            if (operand == 0) {
                m_error = true;
                return;
            }
            result = m_result / operand;
            break;
        default:
            break;
        }
        m_pending = 0;

        char text[MAX_DIGITS + 3];
        if (!formatNumber(result, text)) {
            m_error = true;
            return;
        }
        m_result = result;
    }

    void drawButton(const Button& button, bool pressed) {
        int x = buttonX(button);
        int y = buttonY(button);
        int width = buttonWidth(button);

        unsigned char face = pressed ? PRESSED_FACE_COLOR
                           : isDigit(button.key) || button.key == '.' ? DIGIT_FACE_COLOR
                           : OPERATOR_FACE_COLOR;
        unsigned char topLeft = pressed ? EDGE_DARK_COLOR : EDGE_LIGHT_COLOR;
        unsigned char bottomRight = pressed ? EDGE_LIGHT_COLOR : EDGE_DARK_COLOR;

        drawRect(x, y, width, BUTTON_HEIGHT, face);
        drawRect(x, y, width, 1, topLeft);
        drawRect(x, y, 1, BUTTON_HEIGHT, topLeft);
        drawRect(x, y + BUTTON_HEIGHT - 1, width, 1, bottomRight);
        drawRect(x + width - 1, y, 1, BUTTON_HEIGHT, bottomRight);

        /* A pressed label shifts down and right a pixel, as if the button sank */
        int shift = pressed ? 1 : 0;
        int labelX = x + (width - textLength(button.label) * CHAR_SIZE) / 2 + shift;
        int labelY = y + (BUTTON_HEIGHT - CHAR_SIZE) / 2 + shift;
        drawText(labelX, labelY, button.label, button.key == 'C' ? CLEAR_TEXT_COLOR : TEXT_COLOR);
    }

    void drawDisplay() {
        int x = MARGIN;
        int y = MARGIN;
        int width = WINDOW_WIDTH - 2 * MARGIN;

        /* Sunken frame */
        drawRect(x, y, width, DISPLAY_HEIGHT, DISPLAY_COLOR);
        drawRect(x, y, width, 1, EDGE_DARK_COLOR);
        drawRect(x, y, 1, DISPLAY_HEIGHT, EDGE_DARK_COLOR);

        char text[MAX_DIGITS + 3];
        if (m_error) {
            copyText(text, "Error");
        } else if (m_entering) {
            char* digits = text;
            if (m_entryNegative) {
                *digits++ = '-';
            }
            copyText(digits, m_entryLength > 0 ? m_entry : "0");
        } else {
            formatNumber(m_result, text);
        }

        int textY = y + (DISPLAY_HEIGHT - CHAR_SIZE) / 2;
        if (m_pending != 0) {
            drawChar(x + 4, textY, m_pending == '*' ? 'x' : m_pending, INDICATOR_COLOR);
        }
        drawText(x + width - 4 - textLength(text) * CHAR_SIZE, textY, text, TEXT_COLOR);
    }
};

int main()
{
    Calculator calculator;
    struct gfx_event e;

    while (1) {
        if (gfx_get_event(&e, GFX_EVENT_BLOCKING) != 0) {
            continue;
        }

        switch (e.event) {
        case GFX_EVENT_EXIT:
            return 0;
        case GFX_EVENT_MOUSE:
            calculator.click(e.data, e.data2);
            break;
        case GFX_EVENT_KEYBOARD:
            calculator.keyPressed(e.data);
            break;
        default:
            break;
        }
    }
    return 0;
}
