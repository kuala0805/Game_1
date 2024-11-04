#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <SFML/Audio.hpp>
#include <cmath>
#include <vector>
#include <fstream>
#include <string>
#include <iostream>

constexpr int WINDOW_WIDTH = 1920;  //窗口宽度
constexpr int WINDOW_HEIGHT = 1080; //窗口高度
constexpr float PLAYER_RADIUS = 25.f;   //玩家半径
constexpr float ENEMY_RADIUS = 25.f;    //敌方半径
constexpr float REBOUND_COEFFICIENT = 0.8f; //反弹系数
constexpr float FRICTION_COEFFICIENT = 0.98f;   //摩擦系数
const sf::Vector2f CENTER_ZONE_POSITION(710, 290);  //中心区域左上坐标
const sf::Vector2f CENTER_ZONE_SIZE(500, 500);      //中心区域大小
constexpr int NUM_ENEMIES = 6;  //敌方数量
constexpr float CHARGE_MAX_TIME = 4.f;  //蓄力时间

class TextureManager {
public:
    sf::Texture& getTexture(const std::string& filename) {
        auto& texture = textures[filename];
        if (!texture.loadFromFile(filename)) {
            std::cerr << "Error loading texture from " << filename << std::endl;
            throw std::runtime_error("Failed to load texture!");
        }
        return texture;
    }

private:
    std::map<std::string, sf::Texture> textures;  // Store textures
};

// Usage in GameObject
class GameObject {
public:
    sf::Sprite sprite;
    sf::Vector2f velocity;  // Velocity
    float mass;  // Mass property
    bool isStopped;

    GameObject(float radius, const std::string& textureFile, sf::Vector2f position, TextureManager& textureManager, float m = 1.0f)
            : velocity(0.f, 0.f), mass(m), isStopped(true) {
        sprite.setTexture(textureManager.getTexture(textureFile));
        sprite.setOrigin(radius, radius);
        sprite.setPosition(position);
        sprite.setScale(radius * 2 / sprite.getTexture()->getSize().x, radius * 2 / sprite.getTexture()->getSize().y);
    }

    void draw(sf::RenderWindow& window) const {
        window.draw(sprite);  // Draw the sprite
    }

    void updatePosition(float dt) {
        if (!isStopped) {
            sprite.move(velocity * dt);
            velocity *= FRICTION_COEFFICIENT; // 摩擦减速
            if (std::abs(velocity.x) < 0.01f && std::abs(velocity.y) < 0.01f) {
                isStopped = true;
                velocity = {0.f, 0.f};
            }
        }
    }

    void applyBoundaryCollision() {
        auto bounds = sprite.getGlobalBounds();  // Get the global bounds of the sprite

        // Check for left and right boundary collisions
        if (bounds.left < 0 || bounds.left + bounds.width > WINDOW_WIDTH) {
            velocity.x = -velocity.x * REBOUND_COEFFICIENT;  // Reverse horizontal velocity
            sprite.setPosition(std::max(bounds.width / 2, std::min(sprite.getPosition().x, WINDOW_WIDTH - bounds.width / 2)), sprite.getPosition().y);  // Keep sprite within bounds
        }

        // Check for top and bottom boundary collisions
        if (bounds.top < 0 || bounds.top + bounds.height > WINDOW_HEIGHT) {
            velocity.y = -velocity.y * REBOUND_COEFFICIENT;  // Reverse vertical velocity
            sprite.setPosition(sprite.getPosition().x, std::max(bounds.height / 2, std::min(sprite.getPosition().y, WINDOW_HEIGHT - bounds.height / 2)));  // Keep sprite within bounds
        }
    }

};

class CollisionHandler {
public:
    static void applyCollision(GameObject& a, GameObject& b, sf::Sound& collisionSound) {
        // Get the bounding boxes of the sprites
        sf::FloatRect aBounds = a.sprite.getGlobalBounds();
        sf::FloatRect bBounds = b.sprite.getGlobalBounds();

        // Check for collision using bounding boxes
        if (aBounds.intersects(bBounds)) {
            collisionSound.play();

            auto delta = a.sprite.getPosition() - b.sprite.getPosition();  // Update for sprite
            float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);

            // Calculate the normal and relative velocity
            auto normal = delta / dist;  // Collision normal
            auto relativeVelocity = a.velocity - b.velocity;
            float velocityAlongNormal = relativeVelocity.x * normal.x + relativeVelocity.y * normal.y;

            if (velocityAlongNormal > 0) return;  // Moving apart

            // Update velocities
            sf::Vector2f temp = a.velocity;
            a.velocity = 0.5f * (a.velocity + b.velocity + REBOUND_COEFFICIENT * (b.velocity - a.velocity));
            b.velocity = 0.5f * (b.velocity + temp + REBOUND_COEFFICIENT * (temp - b.velocity));

            a.isStopped = false;
            b.isStopped = false;

            // Debug output
            std::cout << "Collision detected: a velocity: " << a.velocity.x << ", " << a.velocity.y
                      << " | b velocity: " << b.velocity.x << ", " << b.velocity.y << std::endl;
        }
    }
};


class ScoreManager {
private:
    int currentScore;
    int highScore;
    std::string filename;

public:
    ScoreManager(const std::string& file) : currentScore(0), filename(file) {
        highScore = getHighScore();
    }

    void incrementScore() { currentScore++; }
    int getCurrentScore() const { return currentScore; }
    int getHighScore() const { return highScore; }

    void saveScore() {
        std::ofstream file(filename, std::ios::app);
        if (file.is_open()) {
            file << currentScore << "\n";
            file.close();
        }
    }

    int getHighScore() {
        std::ifstream file(filename);
        int highestScore = 0, score;
        if (file.is_open()) {
            while (file >> score) {
                highestScore = std::max(highestScore, score);
            }
            file.close();
        }
        return highestScore;
    }
};

class Game {
private:
    sf::RenderWindow window;
    sf::Font font;
    sf::Text scoreText, highScoreText, finalScoreText, playerCountText, selectionText;
    sf::RectangleShape chargeBar;
    sf::RectangleShape centerZoneBorder;
    sf::SoundBuffer collisionBuffer;
    sf::Sound collisionSound;
    TextureManager textureManager;

    ScoreManager scoreManager;
    std::vector<GameObject> enemies;
    std::vector<GameObject> players;

    int normalCount;
    int specialCount;
    bool isCharging;
    float chargeTime;
    int selectedPlayerIndex;
    bool gameEnded;

public:
    Game() : window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "SFML Game"),
             scoreManager("highscores.txt"),
             normalCount(2), specialCount(2), isCharging(false), chargeTime(0.f),
             selectedPlayerIndex(-1), gameEnded(false) {  // 默认没有选择
        window.setFramerateLimit(60);

        font.loadFromFile("arial.ttf");
        initializeText(scoreText, 24, sf::Vector2f(WINDOW_WIDTH - 200, 20));
        initializeText(highScoreText, 24, sf::Vector2f(WINDOW_WIDTH - 200, 60));
        initializeText(playerCountText, 24, sf::Vector2f(WINDOW_WIDTH - 200, WINDOW_HEIGHT - 80));
        initializeText(finalScoreText, 50, sf::Vector2f(WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 - 50));
        initializeText(selectionText, 24, sf::Vector2f(WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT - 150));

        centerZoneBorder.setSize(CENTER_ZONE_SIZE);
        centerZoneBorder.setOutlineThickness(30);
        centerZoneBorder.setOutlineColor(sf::Color::Red);
        centerZoneBorder.setFillColor(sf::Color::Transparent);
        centerZoneBorder.setPosition(CENTER_ZONE_POSITION);

        chargeBar.setSize(sf::Vector2f(30, 200));
        chargeBar.setFillColor(sf::Color::Yellow);
        chargeBar.setPosition(WINDOW_WIDTH - 60, WINDOW_HEIGHT - 250);

        collisionBuffer.loadFromFile("collision.wav");
        collisionSound.setBuffer(collisionBuffer);

        initializeEnemies();
        initializePlayers();
    }

    void run() {
        while (window.isOpen()) {
            handleEvents();
            if (isCharging) updateCharge();
            updateGameObjects();
            checkCollisions();
            render();
        }
        scoreManager.saveScore();
    }

private:
    void initializeText(sf::Text& text, int size, sf::Vector2f position) {
        text.setFont(font);
        text.setCharacterSize(size);
        text.setPosition(position);
    }

    void initializeEnemies() {
        for (int i = 0; i < NUM_ENEMIES; ++i) {
            sf::Vector2f position;
            bool validPosition = false;

            while (!validPosition) {
                float x = CENTER_ZONE_POSITION.x + std::rand() % static_cast<int>(CENTER_ZONE_SIZE.x);
                float y = CENTER_ZONE_POSITION.y + std::rand() % static_cast<int>(CENTER_ZONE_SIZE.y);
                position = sf::Vector2f(x, y);
                validPosition = true;

                if (position.x - ENEMY_RADIUS < CENTER_ZONE_POSITION.x || position.x + ENEMY_RADIUS > CENTER_ZONE_POSITION.x + CENTER_ZONE_SIZE.x ||
                    position.y - ENEMY_RADIUS < CENTER_ZONE_POSITION.y || position.y + ENEMY_RADIUS > CENTER_ZONE_POSITION.y + CENTER_ZONE_SIZE.y) {
                    validPosition = false;
                    continue;
                }

                for (const auto& enemy : enemies) {
                    float dist = std::sqrt(std::pow(position.x - enemy.sprite.getPosition().x, 2) +
                                           std::pow(position.y - enemy.sprite.getPosition().y, 2));
                    if (dist < ENEMY_RADIUS + ENEMY_RADIUS) {
                        validPosition = false;
                        break;
                    }
                }
            }
            enemies.emplace_back(GameObject(PLAYER_RADIUS, "Images/bird_2.png", position, textureManager));
        }
    }

    void initializePlayers() {
        players = {
                GameObject(PLAYER_RADIUS, "Images/bird_1.png", sf::Vector2f(WINDOW_WIDTH / 2, WINDOW_HEIGHT - 50), textureManager),
                GameObject(PLAYER_RADIUS, "Images/bird_1.png", sf::Vector2f(WINDOW_WIDTH / 2, WINDOW_HEIGHT - 50), textureManager),
                GameObject(PLAYER_RADIUS, "Images/bird_1.png", sf::Vector2f(WINDOW_WIDTH / 2, WINDOW_HEIGHT - 50), textureManager),
                GameObject(PLAYER_RADIUS, "Images/bird_1.png", sf::Vector2f(WINDOW_WIDTH / 2, WINDOW_HEIGHT - 50), textureManager)
        };
    }



    void handleEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Num1) selectedPlayerIndex = 0;
                if (event.key.code == sf::Keyboard::Num2) selectedPlayerIndex = 1;
                if (event.key.code == sf::Keyboard::Num3) selectedPlayerIndex = 2;
                if (event.key.code == sf::Keyboard::Num4) selectedPlayerIndex = 3;
            }

            // 鼠标按下时开始蓄力
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                if (selectedPlayerIndex != -1) {
                    isCharging = true;
                    chargeTime = 0.f;  // 重置蓄力时间
                }
            }

            // 鼠标释放时发射棋子
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left && isCharging) {
                // 获取鼠标位置
                sf::Vector2f mousePos = sf::Vector2f(sf::Mouse::getPosition(window));
                launchPlayer(mousePos);  // 使用鼠标位置发射棋子
                isCharging = false;  // 停止蓄力
            }

            // 空格键事件与管理发射没有直接的操作
            if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Space && isCharging) {
                // 让发射操作与鼠标操作分离
                // 把相关代码移动到鼠标释放事件中
            }
        }
    }

    void updateCharge() {
        if (isCharging) {
            chargeTime += 0.016f; // 假设每帧大约 16 毫秒
            if (chargeTime > CHARGE_MAX_TIME) {
                chargeTime = CHARGE_MAX_TIME;
            }
            chargeBar.setSize(sf::Vector2f(30, chargeTime / CHARGE_MAX_TIME * 200)); // 更新时间条
        }
    }


    void launchPlayer(const sf::Vector2f& mousePos) {
        if (selectedPlayerIndex >= 0) {
            auto& player = players[selectedPlayerIndex];
            // 计算鼠标相对于棋子位置的方向
            sf::Vector2f direction = mousePos - player.sprite.getPosition();
            float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);  // 获取方向长度

            // 归一化方向向量, 避免除以零
            if (length > 0) {
                direction /= length;  // 归一化
            }

            // 根据蓄力时间计算发射速度
            player.velocity = direction * (chargeTime / CHARGE_MAX_TIME * 250.f);
            player.isStopped = false;  // 使棋子开始移动
        }
    }

    void updateGameObjects() {
        for (auto& enemy : enemies) enemy.updatePosition(0.1f);
        for (auto& player : players) player.updatePosition(0.1f);
    }

    void checkCollisions() {
        for (auto& player : players) player.applyBoundaryCollision();
        for (auto& enemy : enemies) enemy.applyBoundaryCollision();

        // 检测每个player与每个enemy的碰撞
        for (size_t i = 0; i < players.size(); ++i) {
            for (size_t j = 0; j < enemies.size(); ++j) {
                CollisionHandler::applyCollision(players[i], enemies[j], collisionSound);
            }
        }

        // 可选：如果需要检测 players 之间的碰撞，可以再加一个循环
        for (size_t i = 0; i < players.size(); ++i) {
            for (size_t j = i + 1; j < players.size(); ++j) {
                CollisionHandler::applyCollision(players[i], players[j], collisionSound);
            }
        }

        for (size_t i = 0; i < enemies.size(); ++i) {
            for (size_t j = i + 1; j < enemies.size(); ++j) {
                CollisionHandler::applyCollision(enemies[i], enemies[j], collisionSound);
            }
        }
    }


    // 修改 render() 方法以绘制精灵
    void render() {
        window.clear();
        window.draw(centerZoneBorder);
        window.draw(scoreText);
        window.draw(highScoreText);
        window.draw(playerCountText);
        window.draw(chargeBar);

        for (const auto& enemy : enemies) enemy.draw(window);  // 调用 draw 方法
        for (const auto& player : players) player.draw(window);  // 调用 draw 方法

        window.draw(selectionText);  // 渲染选择文本
        window.display();
    }
};

int main() {
    Game game;
    game.run();
    return 0;
}