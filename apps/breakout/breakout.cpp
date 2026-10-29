/**
 * @file breakout.cpp
 * @brief Brick breaker game: bounce the ball off the paddle to clear every brick.
 *
 * Left / right arrows move the paddle. Space launches the ball,
 * and starts a new game after a win or game over.
 */

#include <utils/Graphics.hpp>
#include <colors.h>
#include <math.h>

/* Inner window size */
#define GAME_WIDTH 280
#define GAME_HEIGHT 220

/* Score and lives strip along the top */
#define HUD_HEIGHT 12

#define BRICK_ROWS 4
#define BRICK_COLS 8
#define BRICK_WIDTH 32
#define BRICK_HEIGHT 10
#define BRICK_GAP 2
#define BRICKS_TOP (HUD_HEIGHT + 16)
#define BRICKS_LEFT ((GAME_WIDTH - (BRICK_COLS * BRICK_WIDTH + (BRICK_COLS - 1) * BRICK_GAP)) / 2)

#define PADDLE_WIDTH 40
#define PADDLE_HEIGHT 6
#define PADDLE_Y (GAME_HEIGHT - 14)
#define PADDLE_STEP 16

/* Ball position and speed are stored in 1/16 pixels */
#define FP 16
#define BALL_SIZE 4
#define BALL_SPEED_Y (2 * FP)
#define BALL_MAX_SPEED_X (3 * FP)

#define MESSAGE_Y 130
#define FRAME_DELAY_MS 16
#define START_LIVES 3
#define POINTS_PER_BRICK 10

/* Colors are palette indexes */
#define BG_COLOR COLOR_BLACK
/* Dark gray */
#define HUD_COLOR 8
#define PADDLE_COLOR COLOR_WHITE
#define BALL_COLOR COLOR_WHITE
#define TEXT_COLOR COLOR_WHITE

/* Red, orange, yellow and green */
static const unsigned char brick_colors[BRICK_ROWS] = { 39, 43, 44, 48 };

enum GameState {
    STATE_SERVE,    /* ball rests on the paddle until space is pressed */
    STATE_PLAYING,
    STATE_WON,
    STATE_LOST
};

/* Writes label followed by a non-negative number into buffer */
static void formatLabel(char* buffer, const char* label, int value)
{
    while (*label) {
        *buffer++ = *label++;
    }

    char digits[12];
    int count = 0;
    do {
        digits[count++] = '0' + value % 10;
        value /= 10;
    } while (value > 0);

    while (count > 0) {
        *buffer++ = digits[--count];
    }
    *buffer = '\0';
}

static bool rectsOverlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2)
{
    return x1 < x2 + w2 && x2 < x1 + w1 && y1 < y2 + h2 && y2 < y1 + h1;
}

class Breakout : public Window {
public:
    Breakout() : Window(GAME_WIDTH, GAME_HEIGHT, "Breakout", 0) {
        m_serveDirection = 1;
        newGame();
    }

    void input(int key) {
        switch (key) {
        case KEY_LEFT:
            movePaddle(-PADDLE_STEP);
            break;
        case KEY_RIGHT:
            movePaddle(PADDLE_STEP);
            break;
        case ' ':
            if (m_state == STATE_SERVE) {
                clearMessage();
                m_state = STATE_PLAYING;
            } else if (m_state == STATE_WON || m_state == STATE_LOST) {
                newGame();
            }
            break;
        default:
            break;
        }
    }

    /* Advances the ball by one frame */
    void update() {
        if (m_state != STATE_PLAYING) {
            return;
        }

        /* Move one axis at a time, so a collision tells us which direction to reverse */
        m_ballX += m_ballVX;
        if (m_ballX < 0) {
            m_ballX = 0;
            m_ballVX = -m_ballVX;
        } else if (m_ballX > (GAME_WIDTH - BALL_SIZE) * FP) {
            m_ballX = (GAME_WIDTH - BALL_SIZE) * FP;
            m_ballVX = -m_ballVX;
        } else if (hitBricks()) {
            m_ballX -= m_ballVX;
            m_ballVX = -m_ballVX;
        }

        m_ballY += m_ballVY;
        if (m_ballY < HUD_HEIGHT * FP) {
            m_ballY = HUD_HEIGHT * FP;
            m_ballVY = -m_ballVY;
        } else if (hitBricks()) {
            m_ballY -= m_ballVY;
            m_ballVY = -m_ballVY;
        } else if (m_ballVY > 0 && ballOverlaps(m_paddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT)) {
            bounceOffPaddle();
        } else if (m_ballY / FP + BALL_SIZE >= GAME_HEIGHT) {
            loseLife();
            return;
        }

        drawBall();

        if (m_bricksLeft == 0) {
            eraseBall();
            m_state = STATE_WON;
            showMessage("You win! Space to play again");
        }
    }

private:
    unsigned char m_bricks[BRICK_ROWS][BRICK_COLS];
    int m_bricksLeft;
    int m_score;
    int m_lives;
    GameState m_state;

    int m_paddleX;

    int m_ballX, m_ballY;    /* 1/16 pixels */
    int m_ballVX, m_ballVY;  /* 1/16 pixels per frame */
    int m_drawnBallX, m_drawnBallY;
    int m_serveDirection;

    void newGame() {
        for (int row = 0; row < BRICK_ROWS; row++) {
            for (int col = 0; col < BRICK_COLS; col++) {
                m_bricks[row][col] = 1;
            }
        }
        m_bricksLeft = BRICK_ROWS * BRICK_COLS;
        m_score = 0;
        m_lives = START_LIVES;
        m_paddleX = (GAME_WIDTH - PADDLE_WIDTH) / 2;

        placeBallOnPaddle();
        m_drawnBallX = m_ballX / FP;
        m_drawnBallY = m_ballY / FP;

        drawAll();
        serve();
    }

    /* Parks the ball on the paddle until space is pressed, alternating its starting direction */
    void serve() {
        m_state = STATE_SERVE;
        m_serveDirection = -m_serveDirection;
        m_ballVX = m_serveDirection * FP;
        m_ballVY = -BALL_SPEED_Y;

        placeBallOnPaddle();
        drawBall();
        showMessage("Space to launch");
    }

    void placeBallOnPaddle() {
        m_ballX = (m_paddleX + (PADDLE_WIDTH - BALL_SIZE) / 2) * FP;
        m_ballY = (PADDLE_Y - BALL_SIZE) * FP;
    }

    void movePaddle(int dx) {
        if (m_state != STATE_SERVE && m_state != STATE_PLAYING) {
            return;
        }

        int oldX = m_paddleX;
        m_paddleX = CLAMP(m_paddleX + dx, 0, GAME_WIDTH - PADDLE_WIDTH);
        if (m_paddleX == oldX) {
            return;
        }
        moveBlock(oldX, PADDLE_Y, m_paddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, PADDLE_COLOR);

        if (m_state == STATE_SERVE) {
            placeBallOnPaddle();
            drawBall();
        }
    }

    /* The further from the paddle's center the ball lands, the steeper it leaves */
    void bounceOffPaddle() {
        int reach = PADDLE_WIDTH / 2 + BALL_SIZE / 2;
        int offset = (m_ballX / FP + BALL_SIZE / 2) - (m_paddleX + PADDLE_WIDTH / 2);
        offset = CLAMP(offset, -reach, reach);

        m_ballVX = offset * BALL_MAX_SPEED_X / reach;
        m_ballVY = -BALL_SPEED_Y;
        m_ballY = (PADDLE_Y - BALL_SIZE) * FP;
    }

    /* Destroys every brick the ball overlaps, returns whether there were any */
    bool hitBricks() {
        bool hit = false;
        for (int row = 0; row < BRICK_ROWS; row++) {
            for (int col = 0; col < BRICK_COLS; col++) {
                if (m_bricks[row][col] && ballOverlaps(brickX(col), brickY(row), BRICK_WIDTH, BRICK_HEIGHT)) {
                    destroyBrick(row, col);
                    hit = true;
                }
            }
        }
        return hit;
    }

    void destroyBrick(int row, int col) {
        m_bricks[row][col] = 0;
        m_bricksLeft--;
        m_score += POINTS_PER_BRICK;
        fillRect(brickX(col), brickY(row), BRICK_WIDTH, BRICK_HEIGHT, BG_COLOR);
        drawHud();
    }

    void loseLife() {
        eraseBall();
        m_lives--;
        drawHud();

        if (m_lives == 0) {
            m_state = STATE_LOST;
            showMessage("Game over. Space to retry");
            return;
        }
        serve();
    }

    bool ballOverlaps(int x, int y, int width, int height) {
        return rectsOverlap(m_ballX / FP, m_ballY / FP, BALL_SIZE, BALL_SIZE, x, y, width, height);
    }

    int brickX(int col) { return BRICKS_LEFT + col * (BRICK_WIDTH + BRICK_GAP); }
    int brickY(int row) { return BRICKS_TOP + row * (BRICK_HEIGHT + BRICK_GAP); }

    void drawAll() {
        fillRect(0, HUD_HEIGHT, GAME_WIDTH, GAME_HEIGHT - HUD_HEIGHT, BG_COLOR);
        drawHud();

        for (int row = 0; row < BRICK_ROWS; row++) {
            for (int col = 0; col < BRICK_COLS; col++) {
                if (m_bricks[row][col]) {
                    fillRect(brickX(col), brickY(row), BRICK_WIDTH, BRICK_HEIGHT, brick_colors[row]);
                }
            }
        }
        fillRect(m_paddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, PADDLE_COLOR);
    }

    void drawHud() {
        char text[24];

        fillRect(0, 0, GAME_WIDTH, HUD_HEIGHT, HUD_COLOR);
        formatLabel(text, "Score: ", m_score);
        drawText(4, 2, text, TEXT_COLOR);
        formatLabel(text, "Lives: ", m_lives);
        drawText(GAME_WIDTH - 4 - 8 * 8, 2, text, TEXT_COLOR);
    }

    void showMessage(const char* text) {
        int length = 0;
        while (text[length]) {
            length++;
        }

        clearMessage();
        drawText((GAME_WIDTH - length * 8) / 2, MESSAGE_Y, text, TEXT_COLOR);
    }

    void clearMessage() {
        fillRect(0, MESSAGE_Y, GAME_WIDTH, 8, BG_COLOR);
    }

    void drawBall() {
        int x = m_ballX / FP;
        int y = m_ballY / FP;
        bool coveredPaddle = rectsOverlap(m_drawnBallX, m_drawnBallY, BALL_SIZE, BALL_SIZE,
                                          m_paddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT);

        moveBlock(m_drawnBallX, m_drawnBallY, x, y, BALL_SIZE, BALL_SIZE, BALL_COLOR);
        if (coveredPaddle) {
            /* Clearing the old ball cut into the paddle */
            fillRect(m_paddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, PADDLE_COLOR);
            fillRect(x, y, BALL_SIZE, BALL_SIZE, BALL_COLOR);
        }

        m_drawnBallX = x;
        m_drawnBallY = y;
    }

    void eraseBall() {
        fillRect(m_drawnBallX, m_drawnBallY, BALL_SIZE, BALL_SIZE, BG_COLOR);
        if (rectsOverlap(m_drawnBallX, m_drawnBallY, BALL_SIZE, BALL_SIZE,
                         m_paddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT)) {
            fillRect(m_paddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, PADDLE_COLOR);
        }
    }

    /* Draws at the new position, then clears only the part of the old position left uncovered */
    void moveBlock(int oldX, int oldY, int newX, int newY, int width, int height, unsigned char color) {
        fillRect(newX, newY, width, height, color);

        if (!rectsOverlap(oldX, oldY, width, height, newX, newY, width, height)) {
            fillRect(oldX, oldY, width, height, BG_COLOR);
            return;
        }

        /* Old rows above or below the new block */
        if (newY > oldY) {
            fillRect(oldX, oldY, width, newY - oldY, BG_COLOR);
        } else if (newY < oldY) {
            fillRect(oldX, newY + height, width, oldY - newY, BG_COLOR);
        }

        /* Old columns beside the new block, within the rows they share */
        int top = MAX(oldY, newY);
        int bottom = MIN(oldY, newY) + height;
        if (newX > oldX) {
            fillRect(oldX, top, newX - oldX, bottom - top, BG_COLOR);
        } else if (newX < oldX) {
            fillRect(newX + width, top, oldX - newX, bottom - top, BG_COLOR);
        }
    }

    /* The kernel rejects rectangles that reach outside the window, so clip to it */
    void fillRect(int x, int y, int width, int height, unsigned char color) {
        int left = MAX(x, 0);
        int top = MAX(y, 0);
        int right = MIN(x + width, GAME_WIDTH);
        int bottom = MIN(y + height, GAME_HEIGHT);
        if (right <= left || bottom <= top) {
            return;
        }
        gfx_draw_rectangle(left, top, right - left, bottom - top, color);
    }
};

int main()
{
    Breakout game;
    struct gfx_event e;

    while (1) {
        /* Handle everything queued since the last frame; non-zero means the queue is empty */
        while (gfx_get_event(&e, GFX_EVENT_NONBLOCKING) == 0) {
            switch (e.event) {
            case GFX_EVENT_EXIT:
                return 0;
            case GFX_EVENT_KEYBOARD:
                game.input(e.data);
                break;
            default:
                break;
            }
        }

        game.update();
        sleep(FRAME_DELAY_MS);
    }
    return 0;
}
