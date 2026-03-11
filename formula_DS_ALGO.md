以下清單以**重力 N-body 模擬（含 Barnes–Hut、未來 SIMD/平行化）**為中心整理，分成三部分：
1️⃣ 物理公式 2️⃣ 資料結構 3️⃣ 演算法 / 計算方法。
重點是列出**你在這個研究方向中「可能會用到」的典型工具集**，並按從核心到進階排列。

---

# 一、物理公式（Physics Formulas）

## 1. 基本重力與動力學

核心公式來自 **牛頓萬有引力定律**

### (1) 重力大小

[
F = G\frac{m_i m_j}{r^2}
]

---

### (2) 向量形式（N-body simulation常用）

[
\vec F_i = \sum_j G\frac{m_i m_j}{r_{ij}^3}(\vec x_j-\vec x_i)
]

---

### (3) 牛頓第二定律

[
\vec F = m\vec a
]

得到

[
\vec a_i =
\sum_j G\frac{m_j}{r_{ij}^3}(\vec x_j-\vec x_i)
]

---

### (4) 引力位能

[
U = -G\frac{m_i m_j}{r}
]

常用於：

* energy conservation check

---

### (5) 系統總能量

[
E = T + U
]

其中

[
T = \frac12 mv^2
]

---

### (6) 軌道速度

[
v = \sqrt{\frac{GM}{r}}
]

用於初始化 orbital system。

---

### (7) 角動量

[
L = r \times mv
]

---

### (8) gravitational softening

避免 (r\to0)

[
F =
G\frac{m_i m_j}{(r^2+\epsilon^2)^{3/2}}
]

---

## 2. 可能進階使用的公式

### (1) tidal radius

與 **潮汐瓦解事件** 有關

[
r_t \approx R_* \left(\frac{M_{BH}}{M_*}\right)^{1/3}
]

---

### (2) Schwarzschild radius

與 **史瓦西半徑**

[
r_s = \frac{2GM}{c^2}
]

---

### (3) 密度分佈 (stellar cusp)

[
\rho(r) \propto r^{-\gamma}
]

典型：

[
\gamma \approx 1.5\sim2
]

---

### (4) two-body relaxation time

在 **球狀星團** 研究常用

[
t_r \sim
\frac{N}{\ln N}t_{cross}
]

---

# 二、資料結構（Data Structures）

## 1. 基本粒子儲存

### (1) AoS

```cpp
struct Body{
    vec3 pos;
    vec3 vel;
    float mass;
};
```

優點

* 容易理解

缺點

* SIMD 效率差

---

### (2) SoA

```cpp
struct Bodies{
    vector<float> x;
    vector<float> y;
    vector<float> z;
    vector<float> mass;
};
```

優點

* cache friendly
* SIMD friendly

---

## 2. 空間分割資料結構

### (1) Octree

核心於 **Barnes–Hut算法**

```text
root
 ├─ child
 │   ├─ child
```

用於

* hierarchical gravity approximation

---

### (2) Quadtree

2D版本

---

### (3) kd-tree

常用於：

* nearest neighbor search

---

### (4) Bounding Volume

例如

* AABB

---

## 3. Tree node結構

典型：

```cpp
struct Node{
    float mass;
    vec3 center_of_mass;
    Node* children[8];
};
```

---

## 4. 空間索引

可能用到：

* Morton code
* Z-order curve

用於：

* tree construction optimization

---

## 5. 其他輔助資料結構

可能會用到

* vector
* stack（tree traversal）
* queue（BFS traversal）

---

# 三、演算法（Algorithms）

## 1. 核心重力演算法

### (1) Direct N-body

時間複雜度

[
O(N^2)
]

---

### (2) **Barnes–Hut算法**

複雜度

[
O(N\log N)
]

主要概念

* tree approximation
* opening angle (θ)

---

### (3) Fast Multipole Method

**快速多極子法**

複雜度

[
O(N)
]

但實作很複雜。

---

## 2. 積分方法（Time integration）

### (1) Euler

[
x_{t+1}=x_t+v_t dt
]

不穩定。

---

### (2) Leapfrog integrator

N-body 最常用

[
v_{t+1/2}=v_t+a_t dt/2
]

---

### (3) Runge-Kutta

例如

* RK4

---

## 3. Tree traversal

### (1) DFS

深度優先

---

### (2) BFS

廣度優先

---

## 4. 平行化

### (1) **OpenMP**

for loop parallelization

---

### (2) task parallelism

---

### (3) load balancing

常見策略：

* dynamic scheduling
* work stealing

---

## 5. 向量化

### SIMD

**SIMD**

常用

* AVX
* AVX2
* AVX-512

---

## 6. 其他 HPC 技術

### memory optimization

* cache blocking
* memory alignment

---

### GPU computing

* CUDA
* OpenCL

---

# 四、整體架構（典型 N-body pipeline）

```text
initial condition
      ↓
tree construction
      ↓
force calculation
      ↓
time integration
      ↓
update positions
      ↓
repeat
```

---

# 五、最核心的最小集合（你一定會用到）

如果只挑最核心：

### 物理

* Newton gravity
* acceleration equation
* gravitational softening

### 資料結構

* Body struct
* Octree
* vector container

### 演算法

* Barnes–Hut
* Leapfrog integrator
* tree traversal
* OpenMP parallelization

---

