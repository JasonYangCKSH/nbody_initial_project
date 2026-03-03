#include <iostream>
#include <vector>
#include <array>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <SFML/Graphics.hpp>
#include "simulation.h"
#include "body.h"

// --- 模擬 Rust 中的全局狀態 (Global State) ---
namespace Renderer {
    std::atomic<bool> PAUSED(false);
    std::mutex BODIES_LOCK;
    std::mutex QUADTREE_LOCK;
    std::mutex UPDATE_LOCK;
    
    std::vector<Body> SHARED_BODIES;    // 渲染用的 Body 拷貝
    bool NEEDS_UPDATE = false;          // 對應 Rust 的 *lock |= true
}

// --- 物理計算執行緒函數 ---
void simulation_thread(Simulation& sim) {
    while (true) {
        if (Renderer::PAUSED.load(std::memory_order_relaxed)) {
            std::this_thread::yield(); // 讓出 CPU
            continue;
        }

        // 1. 物理步進
        sim.step();

        // 2. 資料同步 (對應 Rust 的 render 函式)
        {
            // 這裡模擬 Rust 的 Lock 機制，將計算完的資料拷貝給渲染執行緒
            std::lock_guard<std::mutex> lock_update(Renderer::UPDATE_LOCK);
            
            // 同步 Bodies
            {
                std::lock_guard<std::mutex> lock_bodies(Renderer::BODIES_LOCK);
                Renderer::SHARED_BODIES = sim.bodies; // C++ vector 的賦值是深拷貝
            }

            // 同步 Quadtree (如果有需要畫出樹狀結構的話)
            {
                std::lock_guard<std::mutex> lock_tree(Renderer::QUADTREE_LOCK);
                // Renderer::SHARED_NODES = sim.octree.nodes; 
            }

            Renderer::NEEDS_UPDATE = true;
        }
    }
}

int main() {
    // 初始化物理引擎
    /*float dt = 0.01f;
    float theta = 0.0f;
    float epsilon = 0.1f;*/
    std::array<float, 3> datas;
    for (int i = 0; i < 3; i++) {
        if (i == 0) std::cout << "please input dt: ";
        if (i == 1) std::cout << "please input theta: ";
        if (i == 2) std::cout << "please input epsilon: ";
        std::cin >> datas[i];
    }
    Simulation sim(datas[0], datas[1], datas[2]);

    // 隨機初始化一些粒子 (與你之前的 main 相同)
    // sim.bodies.push_back(...)
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> disPos(-50.0f, 50.0f);
    size_t bodyNum = 10000;
    for (size_t i = 0; i < bodyNum; i++) {

        Body b;
        b.pos = glm::vec3(disPos(gen), disPos(gen), disPos(gen));
        b.vel = glm::vec3(0.0f); 
        b.mass = 5.0f;
        b.radius = 0.2f;
        sim.bodies.push_back(b);
    }
    // --- 啟動物理執行緒 (std::thread::spawn) ---
    std::thread physics_thread(simulation_thread, std::ref(sim));
    physics_thread.detach(); // 讓他在背景跑

    // --- 渲染執行緒 (主執行緒) ---
    sf::RenderWindow window(sf::VideoMode(900, 900), "N-Body C++ Port");
    sf::VertexArray va(sf::Points);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::P) {
                Renderer::PAUSED = !Renderer::PAUSED; // 暫停開關
            }
        }

        // 取得最新數據進行渲染
        {
            std::lock_guard<std::mutex> lock_bodies(Renderer::BODIES_LOCK);
            va.resize(Renderer::SHARED_BODIES.size());
            for (size_t i = 0; i < Renderer::SHARED_BODIES.size(); ++i) {
                // 這裡需要你的座標轉換邏輯
                float x = Renderer::SHARED_BODIES[i].pos.x + 450; 
                float y = Renderer::SHARED_BODIES[i].pos.y + 450;
                va[i].position = sf::Vector2f(x, y);
                va[i].color = sf::Color::White;
            }
        }

        window.clear(sf::Color::Black);
        window.draw(va);
        window.display();
    }

    return 0;
}