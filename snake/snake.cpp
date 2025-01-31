#include <ncurses.h>
#include <chrono>
#include <deque>
#include <random>
#include <utility>

class SnakeGame {
private:
    static const int WIDTH = 40;   // 增加宽度到40
    static const int HEIGHT = 30;  // 增加高度到30
    std::deque<std::pair<int, int>> snake;
    std::pair<int, int> food;
    int dx, dy;
    int score;
    bool gameOver;
    std::mt19937 rng;
    int gameSpeed;  // 添加游戏速度控制

    void initGame() {
        // 初始化ncurses
        initscr();
        noecho();
        curs_set(0);
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);
        
        // 初始化颜色
        if (has_colors()) {
            start_color();
            init_pair(1, COLOR_GREEN, COLOR_BLACK);   // 蛇身
            init_pair(2, COLOR_RED, COLOR_BLACK);     // 食物
            init_pair(3, COLOR_CYAN, COLOR_BLACK);    // 边界
            init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // 蛇头
        }

        // 初始化蛇（从中间位置开始）
        snake.clear();
        snake.push_back({WIDTH/2, HEIGHT/2});
        snake.push_back({WIDTH/2-1, HEIGHT/2});
        snake.push_back({WIDTH/2-2, HEIGHT/2});
        
        dx = 1;
        dy = 0;
        score = 0;
        gameOver = false;
        gameSpeed = 100;  // 初始速度

        // 初始化随机数生成器
        rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
        generateFood();
    }

    void generateFood() {
        do {
            food.first = rng() % WIDTH;
            food.second = rng() % HEIGHT;
        } while (isSnake(food.first, food.second));
    }

    bool isSnake(int x, int y) {
        for (const auto& segment : snake) {
            if (segment.first == x && segment.second == y) {
                return true;
            }
        }
        return false;
    }

    void processInput() {
        int ch = getch();
        switch (ch) {
            case KEY_UP:
            case 'w':
            case 'W':
                if (dy != 1) { dx = 0; dy = -1; }
                break;
            case KEY_DOWN:
            case 's':
            case 'S':
                if (dy != -1) { dx = 0; dy = 1; }
                break;
            case KEY_LEFT:
            case 'a':
            case 'A':
                if (dx != 1) { dx = -1; dy = 0; }
                break;
            case KEY_RIGHT:
            case 'd':
            case 'D':
                if (dx != -1) { dx = 1; dy = 0; }
                break;
            case 'q':
            case 'Q':
                gameOver = true;
                break;
        }
    }

    void update() {
        if (gameOver) return;

        int newX = snake.front().first + dx;
        int newY = snake.front().second + dy;

        // 检查碰撞
        if (newX < 0 || newX >= WIDTH || newY < 0 || newY >= HEIGHT || 
            isSnake(newX, newY)) {
            gameOver = true;
            return;
        }

        // 移动蛇
        snake.push_front({newX, newY});

        // 检查是否吃到食物
        if (newX == food.first && newY == food.second) {
            score++;
            generateFood();
            // 随着分数增加加快游戏速度
            if (gameSpeed > 50) {
                gameSpeed = 100 - score * 2;
            }
        } else {
            snake.pop_back();
        }
    }

    void render() {
        clear();

        // 绘制边界
        attron(COLOR_PAIR(3));
        for (int i = 0; i < WIDTH+2; i++) {
            mvaddch(0, i, '=');
            mvaddch(HEIGHT+1, i, '=');
        }
        for (int i = 0; i < HEIGHT+2; i++) {
            mvaddch(i, 0, '|');
            mvaddch(i, WIDTH+1, '|');
        }
        attroff(COLOR_PAIR(3));

        // 绘制蛇身
        attron(COLOR_PAIR(1));
        for (size_t i = 1; i < snake.size(); i++) {
            mvaddch(snake[i].second+1, snake[i].first+1, 'o');
        }
        attroff(COLOR_PAIR(1));

        // 绘制蛇头（使用不同颜色）
        attron(COLOR_PAIR(4) | A_BOLD);
        mvaddch(snake.front().second+1, snake.front().first+1, 'O');
        attroff(COLOR_PAIR(4) | A_BOLD);

        // 绘制食物
        attron(COLOR_PAIR(2) | A_BOLD);
        mvaddch(food.second+1, food.first+1, '*');
        attroff(COLOR_PAIR(2) | A_BOLD);

        // 显示分数和控制说明
        mvprintw(HEIGHT+2, 0, "得分: %d    速度: %d", score, 100 - gameSpeed);
        mvprintw(HEIGHT+3, 0, "控制: WASD或方向键移动, Q退出");

        if (gameOver) {
            attron(A_BOLD);
            mvprintw(HEIGHT/2, WIDTH/2-5, "游戏结束!");
            mvprintw(HEIGHT/2+1, WIDTH/2-8, "最终得分: %d", score);
            mvprintw(HEIGHT/2+2, WIDTH/2-10, "按任意键退出...");
            attroff(A_BOLD);
        }

        refresh();
    }

public:
    void run() {
        initGame();

        while (!gameOver) {
            processInput();
            update();
            render();
            napms(gameSpeed);  // 使用动态速度
        }

        // 游戏结束后等待用户按键
        nodelay(stdscr, FALSE);
        getch();
        endwin();
    }
};

int main() {
    SnakeGame game;
    game.run();
    return 0;
}
