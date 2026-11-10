// zombie.cpp — Зомби 3D: Выживание (ФИНАЛ + масштабное обновление)
// Сборка: g++ zombie.cpp -o zombie.exe -I"C:/raylib/include" -L"C:/raylib/lib" -lraylib -lopengl32 -lgdi32 -lwinmm
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <cmath>
#include <random>
#include <string>
#include <algorithm>

// ==================== УТИЛИТЫ ====================
static std::mt19937 g_rng;
inline float rnd(float a, float b){ return std::uniform_real_distribution<float>(a,b)(g_rng); }
inline int   rndi(int a, int b)   { return std::uniform_int_distribution<int>(a,b)(g_rng); }
inline float clampf(float v,float a,float b){ return v<a?a:v>b?b:v; }
inline bool  chance(float p){ return std::uniform_real_distribution<float>(0,1)(g_rng) < p; }

const float WORLD = 95.0f;
const float EYE_H = 1.7f;

// ==================== ОРУЖИЕ ====================
enum WeaponType { W_NONE=-1, W_PISTOL=0, W_SHOTGUN, W_RIFLE, W_SMG, W_FLAME, W_COUNT };

struct WeaponDef {
    const char* name; float damage, cooldown, range; int pellets; float spread; int ammoType; Color color;
};
const WeaponDef WEAPONS[W_COUNT] = {
    {"Пистолет", 28, 0.30f, 40, 1, 0.01f, 0, {255,233,163,255}},
    {"Дробовик", 18, 0.85f, 22, 8, 0.13f, 1, {255,155,61,255}},
    {"Автомат",  22, 0.10f, 55, 1, 0.03f, 0, {127,217,255,255}},
    {"ПП",       14, 0.06f, 35, 1, 0.05f, 0, {90,223,122,255}},
    {"Огнемёт",  26, 0.14f, 12, 3, 0.20f, 2, {255,90,40,255}},
};

// ==================== КРАФТ ====================
struct Recipe {
    const char* name; const char* desc; int type; int result; int amount; int w, m, c, ch;
};
const Recipe RECIPES[] = {
    {"Дробовик",    "Мощь вблизи",   0, W_SHOTGUN, 0, 12, 25, 6, 4},
    {"Автомат",     "Быстрый огонь", 0, W_RIFLE,   0, 15, 40, 4, 6},
    {"ПП",          "Очень быстрый", 0, W_SMG,     0, 10, 30, 8, 3},
    {"Огнемёт",     "Ближний бой",   0, W_FLAME,   0, 8,  45, 12, 18},
    {"Пули ×30",    "Винтовочные",   1, 0, 30, 0, 5, 0, 2},
    {"Картечь ×10", "Для дробовика", 1, 1, 10, 0, 6, 0, 3},
    {"Топливо ×20", "Для огнемёта",  1, 2, 20, 0, 4, 2, 8},
    {"Пули ×100",   "Оптом",         1, 0, 100, 0, 15, 0, 6},
    {"Картечь ×30", "Оптом",         1, 1, 30, 0, 18, 0, 9},
    {"Топливо ×50", "Оптом",         1, 2, 50, 0, 10, 5, 20},
    {"Аптечка",     "+50 HP",        2, 0, 0, 0, 5, 3, 0},
    {"Броня +50",   "Снижает урон",  3, 0, 50, 0, 15, 8, 10},
};
const int RECIPE_COUNT = sizeof(RECIPES)/sizeof(RECIPES[0]);

// ==================== 12 ПОСТРОЕК ====================
struct BuildTypeDef {
    const char* name; float hp; int wood, metal, cloth, chem; Color color; int size;
};
const BuildTypeDef BUILD_TYPES[] = {
    {"Дерево",    250, 3, 0, 0, 0, {160,122,69,255},  1},
    {"Сталь",     700, 0, 8, 0, 0, {138,153,168,255}, 1},
    {"Турель",    180, 3, 8, 0, 0, {76,95,112,255},   1},
    {"Мина",      25,  0, 4, 0, 0, {58,58,58,255},    1},
    {"Электро",   200, 0, 5, 0, 0, {80,140,180,255},  1},
    {"Шипы",      150, 3, 3, 0, 0, {130,100,80,255},  1},
    {"Лекарь",    100, 0, 5, 3, 0, {90,223,122,255},  1},
    {"Генератор", 80,  0, 8, 0, 0, {200,180,60,255},  1},
    {"Баррикада", 500, 10,4, 0, 0, {100,80,50,255},   2},
    {"Огнемёт",   250, 5, 15,3, 0, {180,60,20,255},   1},
    {"Лазер",     300, 5, 20,0, 5, {140,40,180,255},  1},
    {"Факел",     50,  3, 0, 0, 0, {255,180,60,255},  1},
};
const int BUILD_TYPE_COUNT = sizeof(BUILD_TYPES)/sizeof(BUILD_TYPES[0]);

// ==================== ИНВЕНТАРЬ / ИГРОК ====================
struct Inventory {
    int wood = 30, metal = 10, cloth = 5, chem = 2;
    int ammo[3] = {60, 20, 0};
    int medkits = 1;
};

struct Player {
    Vector3 pos = {0, 0, 0};
    float yaw = 0, pitch = 0;
    float hp = 100, maxHp = 100;
    float armor = 0;
    float stamina = 100, maxStamina = 100;
    int currentWeapon = W_PISTOL;
    bool hasWeapon[W_COUNT] = {true, false, false, false, false};
    float weaponCd = 0, recoil = 0, flash = 0, invuln = 0;
    float dashCd = 0, dashTime = 0;
    Vector3 dashDir = {0,0,0};
    Inventory inv;
    int combo = 0; float comboTimer = 0; int bestCombo = 0;
    float damageMultiplier = 1.0f;
    float shakeRecoil = 0, shakeRecoilX = 0, shakeRecoilY = 0;
    float bobPhase = 0, bobAmount = 0;
    bool godMode = false, infiniteAmmo = false, freezeZombies = false, instantBuild = false;
    int nearestBuilding = -1;
};

// ==================== ЗОМБИ ====================
enum ZType { Z_WALKER=0, Z_RUNNER=1, Z_BRUTE=2, Z_BOSS_BRUTE=3, Z_BOSS_QUEEN=4, Z_BOSS_GHOST=5 };

struct Zombie {
    Vector3 pos;
    float hp, maxHp, speed, damage, radius;
    Color color;
    int type;
    bool isBoss = false;
    int phase = 0;
    float attackCd = 0, wobble = 0, hitFlash = 0;
    float abilityCd = 0, castTimer = 0, castMax = 0;
    int castType = 0;
    Vector3 castPos = {0,0,0};
    float phaseTransition = 0, scale = 1.0f, burnTimer = 0;
};

// ==================== ОБЪЕКТЫ ====================
struct Bullet {
    Vector3 pos, vel; float damage, life, radius; Color color;
    bool hostile = false, homing = false, explosive = false;
};

struct Particle {
    Vector3 pos, vel; float life, maxLife, size; Color color;
};

struct Drop {
    Vector3 pos; int type, amount; float life, bob;
};

struct Structure {
    Vector3 pos;
    float hp, maxHp;
    int type = 0;
    int level = 1;
    float cooldown = 0, angle = 0;
    float spawnTime = 0;
    float smokeTimer = 0;
    float buildFlash = 0;
};

struct EnvObj {
    Vector3 pos; int type; float size, rot;
};

struct BloodDecal {
    Vector3 pos; float size, life;
};

// ==================== УРОВНИ ====================
struct LevelDef { int walkers, runners, brutes, bossIdx; };
const LevelDef LEVELS[] = {
    { 6,  0,  0, -1}, {10,  3,  0, -1}, {12,  5,  1, -1}, {14,  7,  2, -1},
    { 8,  4,  1,  0},
    {20, 10,  3, -1}, {22, 12,  4, -1}, {24, 14,  5, -1}, {26, 16,  6, -1},
    {10,  6,  2,  1},
    {30, 18,  8, -1}, {32, 20, 10, -1}, {34, 22, 12, -1}, {36, 24, 14, -1},
    {12,  8,  3,  2},
};
const int LEVEL_COUNT = sizeof(LEVELS)/sizeof(LEVELS[0]);

// ==================== СОСТОЯНИЕ ====================
enum GState { ST_MENU, ST_PLAYING, ST_CRAFT, ST_PAUSE, ST_DEV, ST_GAMEOVER, ST_LEVEL_COMPLETE, ST_VICTORY };

struct Game {
    GState state = ST_MENU;
    Player p;
    Camera3D cam = {0};
    std::vector<Zombie> zombies;
    std::vector<Bullet> bullets;
    std::vector<Particle> particles;
    std::vector<Drop> drops;
    std::vector<Structure> structures;
    std::vector<EnvObj> env;
    std::vector<BloodDecal> bloodDecals;

    int level = 1, score = 0, kills = 0;
    float time = 0, shake = 0, bannerTimer = 0;
    std::string bannerText;
    int craftHover = -1;
    std::string message;
    float messageTimer = 0;
    std::vector<int> spawnQueue;
    float spawnTimer = 0;
    bool gameOver = false, levelStarted = false;
    float levelCompleteTimer = 0;
    bool buildMode = false;
    int buildType = 0;
    Vector3 buildPos = {0,0,0};
    bool buildValid = false;

    void reset();
};

Game G;
Font gFont;

// ==================== FORWARD ====================
void startLevel();
Zombie makeZombie(int type, float difficulty);
void spawnBoss(int bossIdx);
void damagePlayer(float amount);
void killZombie(int idx);
void burst(Vector3 pos, int n, Color color, float power);

// ==================== RESET ====================
void Game::reset() {
    p = Player();
    p.pos = {0, 0, 0};
    zombies.clear(); bullets.clear(); particles.clear();
    drops.clear(); structures.clear();
    env.clear(); bloodDecals.clear();

    for (int i = 0; i < 60; i++) {
        Vector3 pos = {rnd(-WORLD+5, WORLD-5), 0, rnd(-WORLD+5, WORLD-5)};
        if (Vector3Length({pos.x, 0, pos.z}) < 8) continue;
        env.push_back({pos, 0, rnd(2.0f, 4.0f), rnd(0, 6.28f)});
    }
    for (int i = 0; i < 30; i++) {
        Vector3 pos = {rnd(-WORLD+5, WORLD-5), 0, rnd(-WORLD+5, WORLD-5)};
        if (Vector3Length({pos.x, 0, pos.z}) < 6) continue;
        env.push_back({pos, 1, rnd(1.0f, 2.5f), rnd(0, 6.28f)});
    }
    for (int i = 0; i < 12; i++) {
        Vector3 pos = {rnd(-WORLD+10, WORLD-10), 0, rnd(-WORLD+10, WORLD-10)};
        if (Vector3Length({pos.x, 0, pos.z}) < 15) continue;
        env.push_back({pos, 2, rnd(3.0f, 6.0f), rnd(0, 6.28f)});
    }

    level = 1; score = 0; kills = 0;
    time = 0; shake = 0; bannerTimer = 0;
    craftHover = -1; message = ""; messageTimer = 0;
    spawnQueue.clear(); spawnTimer = 0;
    gameOver = false; levelStarted = false; levelCompleteTimer = 0;
    buildMode = false; buildType = 0;

    cam.position = {0, EYE_H, 0};
    cam.target = {0, EYE_H, 1};
    cam.up = {0, 1, 0};
    cam.fovy = 75;
    cam.projection = CAMERA_PERSPECTIVE;

    state = ST_PLAYING;
    startLevel();
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ ====================
Vector3 ForwardDir(float yaw, float pitch) {
    return { sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch) };
}

void burst(Vector3 pos, int n, Color color, float power = 4.0f) {
    for (int i = 0; i < n; i++) {
        float a = rnd(0, 6.28f), b = rnd(-1.5f, 1.5f);
        Vector3 v = {cosf(a)*rnd(1,power), b*rnd(1,power), sinf(a)*rnd(1,power)};
        float life = rnd(0.3f, 0.8f);
        G.particles.push_back({pos, v, life, life, rnd(0.05f, 0.15f), color});
    }
}

void spawnDrop(Vector3 pos, int type, int amount) {
    G.drops.push_back({pos, type, amount, 20.0f, rnd(0, 6.28f)});
}

// ==================== СПАВН ====================
Zombie makeZombie(int type, float difficulty) {
    Zombie z;
    z.type = type; z.wobble = rnd(0, 6.28f);
    switch (type) {
        case Z_WALKER: z.hp = 60*difficulty; z.speed = 2.2f; z.damage = 8; z.radius = 0.45f; z.color = {78,140,63,255}; break;
        case Z_RUNNER: z.hp = 40*difficulty; z.speed = 4.8f; z.damage = 6; z.radius = 0.35f; z.color = {156,107,47,255}; break;
        case Z_BRUTE:  z.hp = 220*difficulty; z.speed = 1.4f; z.damage = 20; z.radius = 0.9f; z.color = {122,58,107,255}; break;
        case Z_BOSS_BRUTE: z.hp = 2500; z.speed = 2.0f; z.damage = 30; z.radius = 2.0f; z.color = {122,26,42,255}; z.isBoss = true; break;
        case Z_BOSS_QUEEN: z.hp = 4000; z.speed = 1.8f; z.damage = 22; z.radius = 1.7f; z.color = {90,42,138,255}; z.isBoss = true; break;
        case Z_BOSS_GHOST: z.hp = 6000; z.speed = 3.0f; z.damage = 25; z.radius = 1.5f; z.color = {170,184,200,255}; z.isBoss = true; break;
    }
    z.maxHp = z.hp;
    return z;
}

void spawnBoss(int bossIdx) {
    Zombie z;
    if (bossIdx == 0) z = makeZombie(Z_BOSS_BRUTE, 1);
    else if (bossIdx == 1) z = makeZombie(Z_BOSS_QUEEN, 1);
    else z = makeZombie(Z_BOSS_GHOST, 1);
    float scale = 1.0f + (G.level - (bossIdx == 0 ? 5 : bossIdx == 1 ? 10 : 15)) * 0.15f;
    z.hp *= scale; z.maxHp = z.hp;
    float angle = rnd(0, 6.28f);
    z.pos = {G.p.pos.x + cosf(angle)*40, 0, G.p.pos.z + sinf(angle)*40};
    z.pos.x = clampf(z.pos.x, -WORLD+5, WORLD-5);
    z.pos.z = clampf(z.pos.z, -WORLD+5, WORLD-5);
    G.zombies.push_back(z);
    G.bannerTimer = 3.0f;
    G.bannerText = bossIdx == 0 ? "БОСС: ГРОМИЛА" : bossIdx == 1 ? "БОСС: КОРОЛЕВА" : "БОСС: ПРИЗРАК";
    G.shake = 15;
}

void startLevel() {
    int idx = (G.level <= LEVEL_COUNT) ? G.level - 1 : (LEVEL_COUNT + (G.level - LEVEL_COUNT) % LEVEL_COUNT) - 1;
    LevelDef comp = LEVELS[idx];
    if (G.level > LEVEL_COUNT) {
        int bonus = G.level - LEVEL_COUNT;
        comp = {20 + bonus*3, 10 + bonus*2, 3 + bonus, (G.level % 5 == 0) ? (G.level/5) % 3 : -1};
    }
    G.spawnQueue.clear();
    for (int i = 0; i < comp.walkers; i++) G.spawnQueue.push_back(Z_WALKER);
    for (int i = 0; i < comp.runners; i++) G.spawnQueue.push_back(Z_RUNNER);
    for (int i = 0; i < comp.brutes; i++) G.spawnQueue.push_back(Z_BRUTE);
    for (int i = (int)G.spawnQueue.size()-1; i > 0; i--) {
        int j = rndi(0, i);
        std::swap(G.spawnQueue[i], G.spawnQueue[j]);
    }
    G.spawnTimer = 1.0f;
    if (comp.bossIdx >= 0) spawnBoss(comp.bossIdx);
    G.bannerText = TextFormat("УРОВЕНЬ %d", G.level);
    G.bannerTimer = 2.5f;
    G.levelStarted = true;
    G.levelCompleteTimer = 0;
}

// ==================== УРОН ====================
void damagePlayer(float amount) {
    if (G.p.invuln > 0 || G.gameOver || G.p.godMode) return;
    if (G.p.armor > 0) {
        float absorb = fminf(G.p.armor, amount * 0.5f);
        G.p.armor -= absorb; amount -= absorb;
    }
    G.p.hp -= amount;
    G.p.invuln = 0.3f; G.p.flash = 0.3f;
    G.shake = std::min(G.shake + 5, 12.f);
    burst({G.p.pos.x, 1.0f, G.p.pos.z}, 8, {200,50,50,255}, 3);
    if (G.p.hp <= 0) { G.p.hp = 0; G.gameOver = true; G.state = ST_GAMEOVER; }
}

void killZombie(int idx) {
    Zombie& z = G.zombies[idx];
    float mul = 1 + std::min(G.p.combo, 10) * 0.1f;
    G.score += (int)((z.isBoss ? 500 : z.type == Z_BRUTE ? 40 : 10) * mul);
    G.kills++;
    G.p.combo++; G.p.comboTimer = 2.5f;
    if (G.p.combo > G.p.bestCombo) G.p.bestCombo = G.p.combo;
    burst(z.pos, z.isBoss ? 60 : 15, {139,26,26,255}, z.isBoss ? 8 : 4);
    G.bloodDecals.push_back({z.pos, z.radius * 1.5f, 25.0f});
    if (G.bloodDecals.size() > 80) G.bloodDecals.erase(G.bloodDecals.begin());
    if (z.isBoss) {
        for (int i = 0; i < 8; i++) spawnDrop({z.pos.x + rnd(-2,2), 0.5f, z.pos.z + rnd(-2,2)}, rndi(0,3), rndi(8,20));
        spawnDrop(z.pos, 4, 2);
    } else {
        int type = rndi(0,3);
        int amount = z.type == Z_BRUTE ? rndi(4,8) : rndi(1,3);
        spawnDrop(z.pos, type, amount);
        if (chance(0.1f)) spawnDrop(z.pos, 4, 1);
    }
    G.zombies.erase(G.zombies.begin() + idx);
}

// ==================== СТРЕЛЬБА ====================
void shoot() {
    Player& p = G.p;
    if (p.currentWeapon < 0) return;
    const WeaponDef& w = WEAPONS[p.currentWeapon];
    if (!p.infiniteAmmo && p.inv.ammo[w.ammoType] <= 0) return;
    if (!p.infiniteAmmo) p.inv.ammo[w.ammoType]--;
    p.weaponCd = w.cooldown; p.recoil = 0.08f; p.flash = 0.05f;
    p.shakeRecoil = 1.0f;
    p.shakeRecoilX = rnd(-0.15f, 0.15f);
    p.shakeRecoilY = rnd(-0.25f, 0.1f);
    G.shake = std::min(G.shake + (w.pellets > 1 ? 3.f : 1.f), 5.f);
    Vector3 eye = {p.pos.x, EYE_H, p.pos.z};
    Vector3 fwd = ForwardDir(p.yaw, p.pitch);
    for (int i = 0; i < w.pellets; i++) {
        Vector3 dir = {fwd.x + rnd(-w.spread, w.spread), fwd.y + rnd(-w.spread, w.spread), fwd.z + rnd(-w.spread, w.spread)};
        dir = Vector3Normalize(dir);
        Bullet b;
        b.pos = {eye.x + fwd.x*0.3f, eye.y - 0.1f + fwd.y*0.3f, eye.z + fwd.z*0.3f};
        b.vel = Vector3Scale(dir, 80);
        b.damage = w.damage * p.damageMultiplier;
        b.life = w.range / 80.0f;
        b.radius = w.ammoType == 1 ? 0.1f : 0.06f;
        b.color = w.color; b.hostile = false;
        G.bullets.push_back(b);
    }
}

// ==================== КРАФТ ====================
bool canCraft(const Recipe& r) {
    return G.p.inv.wood >= r.w && G.p.inv.metal >= r.m && G.p.inv.cloth >= r.c && G.p.inv.chem >= r.ch;
}

void doCraft(int idx) {
    const Recipe& r = RECIPES[idx];
    if (!canCraft(r)) return;
    G.p.inv.wood -= r.w; G.p.inv.metal -= r.m; G.p.inv.cloth -= r.c; G.p.inv.chem -= r.ch;
    if (r.type == 0) {
        G.p.hasWeapon[r.result] = true;
        G.p.currentWeapon = r.result;
        G.message = std::string("Создано: ") + WEAPONS[r.result].name;
    } else if (r.type == 1) {
        G.p.inv.ammo[r.result] += r.amount;
        G.message = std::string("Создано: ") + r.name;
    } else if (r.type == 2) {
        G.p.inv.medkits += 1; G.message = "Создана аптечка";
    } else if (r.type == 3) {
        G.p.armor = fminf(100.0f, G.p.armor + r.amount);
        G.message = "Броня добавлена";
    }
    G.messageTimer = 2.0f;
}

// ==================== ИГРОК ====================
void updatePlayer(float dt) {
    Player& p = G.p;
    if (p.weaponCd > 0) p.weaponCd -= dt;
    if (p.recoil > 0) p.recoil -= dt * 2;
    if (p.flash > 0) p.flash -= dt;
    if (p.invuln > 0) p.invuln -= dt;
    if (p.dashCd > 0) p.dashCd -= dt;
    if (p.dashTime > 0) p.dashTime -= dt;
    if (p.comboTimer > 0) { p.comboTimer -= dt; if (p.comboTimer <= 0) p.combo = 0; }
    if (p.shakeRecoil > 0) p.shakeRecoil -= dt * 7.0f;

    Vector2 md = GetMouseDelta();
    p.yaw -= md.x * 0.003f;
    p.pitch -= md.y * 0.003f;
    p.pitch = clampf(p.pitch, -1.4f, 1.4f);

    float speed = 6.0f;
    bool sprinting = IsKeyDown(KEY_LEFT_SHIFT) && p.stamina > 5;
    if (sprinting) { speed = 10.0f; p.stamina = std::max(0.f, p.stamina - 25*dt); }
    else p.stamina = std::min(p.maxStamina, p.stamina + 15*dt);

    float dx = 0, dz = 0;
    if (IsKeyDown(KEY_W)) dz += 1;
    if (IsKeyDown(KEY_S)) dz -= 1;
    if (IsKeyDown(KEY_A)) dx -= 1;
    if (IsKeyDown(KEY_D)) dx += 1;

    Vector3 move = {0,0,0};
    if (dx != 0 || dz != 0) {
        Vector3 fwd = {sinf(p.yaw), 0, cosf(p.yaw)};
        Vector3 right = {-cosf(p.yaw), 0, sinf(p.yaw)};
        move = Vector3Add(Vector3Scale(fwd, dz), Vector3Scale(right, dx));
        if (Vector3Length(move) > 0) move = Vector3Normalize(move);
    }

    if (IsKeyPressed(KEY_SPACE) && p.dashCd <= 0) {
        p.dashDir = (Vector3Length(move) > 0) ? move : Vector3{sinf(p.yaw), 0, cosf(p.yaw)};
        p.dashTime = 0.18f; p.dashCd = 1.5f;
        p.invuln = std::max(p.invuln, 0.25f);
    }
    if (p.dashTime > 0) { speed = 25.0f; move = p.dashDir; }

    float moveLen = Vector3Length(move);
    bool isMoving = moveLen > 0.1f && p.dashTime <= 0;
    float targetBob = isMoving ? 1.0f : 0.0f;
    p.bobAmount += (targetBob - p.bobAmount) * dt * 8.0f;
    if (isMoving) p.bobPhase += dt * (sprinting ? 15.0f : 10.0f);

    Vector3 newPos = Vector3Add(p.pos, Vector3Scale(move, speed*dt));
    newPos.x = clampf(newPos.x, -WORLD, WORLD);
    newPos.z = clampf(newPos.z, -WORLD, WORLD);
    bool blocked = false;
    for (auto& s : G.structures) {
        if (fabsf(newPos.x - s.pos.x) < 1.3f && fabsf(newPos.z - s.pos.z) < 1.3f) { blocked = true; break; }
    }
    if (!blocked) p.pos = newPos;

    G.cam.position = {p.pos.x, EYE_H, p.pos.z};
    Vector3 fwd = ForwardDir(p.yaw, p.pitch);
    G.cam.target = Vector3Add(G.cam.position, fwd);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && p.weaponCd <= 0 && p.currentWeapon >= 0) shoot();
    for (int i = 0; i < W_COUNT; i++) {
        if (IsKeyPressed(KEY_ONE + i) && p.hasWeapon[i]) p.currentWeapon = i;
    }
    if (IsKeyPressed(KEY_E) && p.inv.medkits > 0 && p.hp < p.maxHp) {
        p.inv.medkits--; p.hp = std::min(p.maxHp, p.hp + 50);
        G.message = "Аптечка использована"; G.messageTimer = 1.5f;
    }

    // Поиск ближайшей постройки
    p.nearestBuilding = -1;
    float nearestDist = 3.5f * 3.5f;
    for (int i = 0; i < (int)G.structures.size(); i++) {
        float dd = (G.structures[i].pos.x - p.pos.x)*(G.structures[i].pos.x - p.pos.x) +
                   (G.structures[i].pos.z - p.pos.z)*(G.structures[i].pos.z - p.pos.z);
        if (dd < nearestDist) { nearestDist = dd; p.nearestBuilding = i; }
    }

    if (p.nearestBuilding >= 0 && !G.buildMode) {
        Structure& s = G.structures[p.nearestBuilding];

        // U — улучшение
        if (IsKeyPressed(KEY_U) && s.level < 3) {
            int wCost = s.level == 1 ? 8 : 15;
            int mCost = s.level == 1 ? 8 : 20;
            if (p.inv.wood >= wCost && p.inv.metal >= mCost) {
                p.inv.wood -= wCost; p.inv.metal -= mCost;
                s.level++;
                s.maxHp *= 1.6f;
                s.hp = s.maxHp;
                s.buildFlash = 0.6f;
                burst(s.pos, 25, GOLD, 6);
                G.message = TextFormat("Постройка улучшена до ур. %d", s.level);
                G.messageTimer = 2.0f;
            } else {
                G.message = TextFormat("Нужно: Д%d М%d", wCost, mCost);
                G.messageTimer = 1.5f;
            }
        }

        // R — ремонт
        if (IsKeyPressed(KEY_R) && s.hp < s.maxHp) {
            float missing = s.maxHp - s.hp;
            int cost = (int)ceilf(missing / 40.0f);
            if (cost < 1) cost = 1;
            if (p.inv.metal >= cost) {
                p.inv.metal -= cost;
                s.hp = s.maxHp;
                burst(s.pos, 15, GREEN, 3);
                G.message = TextFormat("Починено за %d металла", cost);
                G.messageTimer = 2.0f;
            } else {
                G.message = TextFormat("Нужно %d металла", cost);
                G.messageTimer = 1.5f;
            }
        }
    }

    if (IsKeyPressed(KEY_B)) G.buildMode = !G.buildMode;
    if (G.buildMode) {
        for (int i = 0; i < BUILD_TYPE_COUNT && i < 9; i++) {
            if (IsKeyPressed(KEY_ONE + i)) G.buildType = i;
        }
        if (IsKeyPressed(KEY_ZERO)) G.buildType = 9;
        if (IsKeyPressed(KEY_MINUS)) G.buildType = 10;
        if (IsKeyPressed(KEY_EQUAL)) G.buildType = 11;
        float wheel = GetMouseWheelMove();
        if (wheel > 0) G.buildType = (G.buildType - 1 + BUILD_TYPE_COUNT) % BUILD_TYPE_COUNT;
        if (wheel < 0) G.buildType = (G.buildType + 1) % BUILD_TYPE_COUNT;
        Vector3 f2 = {sinf(p.yaw), 0, cosf(p.yaw)};
        Vector3 target = {p.pos.x + f2.x*3, 1, p.pos.z + f2.z*3};
        G.buildPos = {roundf(target.x), 1, roundf(target.z)};
        const BuildTypeDef& bt = BUILD_TYPES[G.buildType];
        bool valid = true;
        for (auto& s : G.structures) {
            if (fabsf(G.buildPos.x - s.pos.x) < 1.5f && fabsf(G.buildPos.z - s.pos.z) < 1.5f) { valid = false; break; }
        }
        if (!p.instantBuild) {
            if (p.inv.wood < bt.wood || p.inv.metal < bt.metal || p.inv.cloth < bt.cloth || p.inv.chem < bt.chem) valid = false;
        }
        G.buildValid = valid;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && valid) {
            Structure s;
            s.pos = G.buildPos;
            s.hp = bt.hp; s.maxHp = bt.hp;
            s.type = G.buildType;
            s.spawnTime = G.time;
            s.level = 1;
            s.buildFlash = 0.6f;
            G.structures.push_back(s);
            if (!p.instantBuild) {
                p.inv.wood -= bt.wood; p.inv.metal -= bt.metal;
                p.inv.cloth -= bt.cloth; p.inv.chem -= bt.chem;
            }
            burst(G.buildPos, 12, bt.color);
        }
    }
}

// ==================== BOSS ====================
void updateBoss(Zombie& z, float dt) {
    if (z.phaseTransition > 0) {
        z.phaseTransition -= dt;
        if (z.phaseTransition <= 0) {
            z.phase++; z.scale = 1.0f + z.phase * 0.15f; z.speed *= 1.25f; z.abilityCd = 1.0f;
            burst(z.pos, 40, z.color, 6); G.shake = 15;
        }
        return;
    }
    float pct = z.hp / z.maxHp;
    if (z.phase == 0 && pct < 0.66f) { z.phaseTransition = 1.5f; G.shake = 12; burst(z.pos, 50, {255,200,50,255}, 8); return; }
    if (z.phase == 1 && pct < 0.33f) { z.phaseTransition = 1.5f; G.shake = 18; burst(z.pos, 60, {255,50,50,255}, 10); return; }
    if (z.abilityCd > 0) z.abilityCd -= dt;
    Vector3 toP = {G.p.pos.x - z.pos.x, 0, G.p.pos.z - z.pos.z};
    float dist = Vector3Length(toP);
    Vector3 dir = (dist > 0.1f) ? Vector3Scale(toP, 1/dist) : Vector3{0,0,0};
    if (z.castTimer > 0) {
        z.castTimer -= dt;
        if (z.castTimer <= 0) {
            if (z.castType == 1) {
                if (Vector3Length({G.p.pos.x - z.castPos.x, 0, G.p.pos.z - z.castPos.z}) < 5.0f) damagePlayer(35);
                for (int s = (int)G.structures.size()-1; s >= 0; s--) {
                    if (Vector3Length({G.structures[s].pos.x - z.castPos.x, 0, G.structures[s].pos.z - z.castPos.z}) < 5.0f) {
                        G.structures[s].hp -= 150;
                        if (G.structures[s].hp <= 0) {
                            // ИСПРАВЛЕНО: цвет из BUILD_TYPES по типу
                            burst(G.structures[s].pos, 40, BUILD_TYPES[G.structures[s].type].color, 8);
                            G.structures.erase(G.structures.begin()+s);
                        }
                    }
                }
                burst(z.castPos, 40, {255,48,96,255}, 8); G.shake = 12;
            } else if (z.castType == 2) { z.speed = 12.0f; z.abilityCd = 1.0f; }
            else if (z.castType == 3) {
                for (int i = -2; i <= 2; i++) {
                    float a = atan2f(toP.x, toP.z) + i*0.15f;
                    Bullet b; b.pos = {z.pos.x, 1.5f, z.pos.z};
                    b.vel = {sinf(a)*18, 0, cosf(a)*18};
                    b.damage = 15; b.life = 3.0f; b.radius = 0.2f;
                    b.color = {255,138,224,255}; b.hostile = true;
                    G.bullets.push_back(b);
                }
            } else if (z.castType == 4) {
                for (int i = 0; i < 4; i++) {
                    float a = (i/4.0f)*6.28f;
                    Zombie m = makeZombie(Z_RUNNER, 1.0f);
                    m.pos = {z.pos.x + cosf(a)*3, 0, z.pos.z + sinf(a)*3};
                    G.zombies.push_back(m);
                }
            } else if (z.castType == 5) {
                Bullet b; b.pos = {z.pos.x, 1.5f, z.pos.z};
                b.vel = Vector3Scale(dir, 12);
                b.damage = 22; b.life = 4.0f; b.radius = 0.25f;
                b.color = {170,184,200,255}; b.hostile = true; b.homing = true;
                G.bullets.push_back(b);
            } else if (z.castType == 6) {
                float a = rnd(0, 6.28f), d = rnd(8, 15);
                Vector3 np = {G.p.pos.x + cosf(a)*d, 0, G.p.pos.z + sinf(a)*d};
                np.x = clampf(np.x, -WORLD+2, WORLD-2);
                np.z = clampf(np.z, -WORLD+2, WORLD-2);
                burst(z.pos, 20, z.color, 4); z.pos = np; burst(z.pos, 20, z.color, 4);
            }
            z.castType = 0;
        }
        return;
    }
    if (z.castType == 0 && z.speed > 10.0f && z.abilityCd <= 0) {
        z.speed = (z.type == Z_BOSS_BRUTE) ? 2.0f : (z.type == Z_BOSS_QUEEN) ? 1.8f : 3.0f;
        z.speed *= (1.0f + z.phase * 0.25f);
    }
    if (z.abilityCd <= 0 && dist < 30.0f) {
        z.abilityCd = 2.0f;
        if (z.type == Z_BOSS_BRUTE) {
            if (chance(0.6f)) { z.castType = 1; z.castTimer = z.castMax = 1.2f; z.castPos = {G.p.pos.x, 0, G.p.pos.z}; }
            else { z.castType = 2; z.castTimer = z.castMax = 0.8f; }
        } else if (z.type == Z_BOSS_QUEEN) {
            if (chance(0.6f)) { z.castType = 3; z.castTimer = z.castMax = 0.6f; }
            else { z.castType = 4; z.castTimer = z.castMax = 1.0f; }
        } else if (z.type == Z_BOSS_GHOST) {
            float r = rnd(0, 1);
            if (r < 0.4f) { z.castType = 5; z.castTimer = z.castMax = 0.7f; }
            else { z.castType = 6; z.castTimer = z.castMax = 0.4f; }
        }
    }
    if (dist > 1.5f) z.pos = Vector3Add(z.pos, Vector3Scale(dir, z.speed*dt));
    if (dist < z.radius + 1.0f && z.attackCd <= 0) { z.attackCd = 1.0f; damagePlayer(z.damage); }
    if (z.attackCd > 0) z.attackCd -= dt;
    if (z.type == Z_BOSS_QUEEN && z.phase >= 1 && chance(0.005f)) {
        Zombie m = makeZombie(Z_WALKER, 1.0f);
        m.pos = {z.pos.x + rnd(-4,4), 0, z.pos.z + rnd(-4,4)};
        G.zombies.push_back(m);
    }
    if (z.type == Z_BOSS_BRUTE && z.phase >= 2 && z.speed < 5.0f) z.speed = 4.5f;
}

// ==================== ЗОМБИ ====================
void updateZombies(float dt) {
    if (G.p.freezeZombies) return;
    for (int i = (int)G.zombies.size()-1; i >= 0; i--) {
        auto& z = G.zombies[i];
        z.wobble += dt * (z.isBoss ? 4.0f : 8.0f);
        if (z.hitFlash > 0) z.hitFlash -= dt;
        if (z.burnTimer > 0) {
            z.burnTimer -= dt;
            z.hp -= 8 * dt;
            if (chance(0.3f)) burst(z.pos, 2, {255,120,40,255}, 2);
            if (z.hp <= 0) { killZombie(i); continue; }
        }
        if (z.isBoss) { updateBoss(z, dt); continue; }
        Vector3 toP = {G.p.pos.x - z.pos.x, 0, G.p.pos.z - z.pos.z};
        float dist = Vector3Length(toP);
        Vector3 dir = (dist > 0.1f) ? Vector3Scale(toP, 1/dist) : Vector3{0,0,0};
        Vector3 newPos = Vector3Add(z.pos, Vector3Scale(dir, z.speed*dt));
        bool blocked = false;
        for (int s = (int)G.structures.size()-1; s >= 0; s--) {
            if (fabsf(newPos.x - G.structures[s].pos.x) < 1.3f &&
                fabsf(newPos.z - G.structures[s].pos.z) < 1.3f) {
                if (z.attackCd <= 0) {
                    z.attackCd = 0.8f;
                    G.structures[s].hp -= z.damage * 2;
                    if (G.structures[s].hp <= 0) {
                        // ИСПРАВЛЕНО: цвет из BUILD_TYPES по типу
                        burst(G.structures[s].pos, 40, BUILD_TYPES[G.structures[s].type].color, 8);
                        G.structures.erase(G.structures.begin()+s);
                    }
                }
                blocked = true; break;
            }
        }
        if (!blocked) z.pos = newPos;
        if (dist < z.radius + 1.0f && z.attackCd <= 0) { z.attackCd = 0.8f; damagePlayer(z.damage); }
        if (z.attackCd > 0) z.attackCd -= dt;
    }
}

// ==================== ПУЛИ ====================
void updateBullets(float dt) {
    for (int i = (int)G.bullets.size()-1; i >= 0; i--) {
        auto& b = G.bullets[i];
        b.life -= dt;
        if (b.life <= 0) { G.bullets.erase(G.bullets.begin()+i); continue; }
        if (b.homing && b.hostile) {
            Vector3 toP = {G.p.pos.x - b.pos.x, 1.0f - b.pos.y, G.p.pos.z - b.pos.z};
            float d = Vector3Length(toP);
            if (d > 0.1f) {
                toP = Vector3Scale(toP, 1/d);
                float sp = Vector3Length(b.vel);
                b.vel = Vector3Scale(toP, sp);
            }
        }
        b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));
        if (b.pos.x < -WORLD || b.pos.x > WORLD || b.pos.z < -WORLD || b.pos.z > WORLD) {
            G.bullets.erase(G.bullets.begin()+i); continue;
        }
        bool hit = false;
        if (b.hostile) {
            Vector3 eye = {G.p.pos.x, 1.0f, G.p.pos.z};
            if (Vector3Distance(b.pos, eye) < 0.8f + b.radius) { damagePlayer(b.damage); hit = true; }
        } else {
            for (int j = (int)G.zombies.size()-1; j >= 0; j--) {
                auto& z = G.zombies[j];
                Vector3 bp = {z.pos.x, z.radius * 1.5f, z.pos.z};
                if (Vector3Distance(b.pos, bp) < z.radius * 1.8f + b.radius) {
                    float dmg = b.damage;
                    if (z.isBoss) dmg *= 0.5f;
                    z.hp -= dmg; z.hitFlash = 0.1f;
                    if (b.explosive) {
                        for (int k = (int)G.zombies.size()-1; k >= 0; k--) {
                            if (k == j) continue;
                            auto& z2 = G.zombies[k];
                            if (Vector3Distance(b.pos, z2.pos) < 3.0f) {
                                z2.hp -= dmg * 0.5f;
                                z2.burnTimer = 3.0f; z2.hitFlash = 0.1f;
                                if (z2.hp <= 0) killZombie(k);
                            }
                        }
                        z.burnTimer = 3.0f;
                        burst(b.pos, 20, {255,120,40,255}, 6);
                    } else burst(b.pos, 4, {200,40,40,255}, 2);
                    hit = true;
                    if (z.hp <= 0) killZombie(j);
                    break;
                }
            }
        }
        if (hit) G.bullets.erase(G.bullets.begin()+i);
    }
}

// ==================== ЧАСТИЦЫ / ДРОПЫ ====================
void updateParticles(float dt) {
    for (int i = (int)G.particles.size()-1; i >= 0; i--) {
        auto& p = G.particles[i];
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        p.vel.y -= 15.0f * dt;
        p.life -= dt;
        if (p.life <= 0) G.particles.erase(G.particles.begin()+i);
    }
}

void updateDrops(float dt) {
    for (int i = (int)G.drops.size()-1; i >= 0; i--) {
        auto& d = G.drops[i];
        d.life -= dt; d.bob += dt * 3.0f;
        if (d.life <= 0) { G.drops.erase(G.drops.begin()+i); continue; }
        Vector3 toP = {G.p.pos.x - d.pos.x, 0, G.p.pos.z - d.pos.z};
        float dist = Vector3Length(toP);
        if (dist < 1.5f) {
            switch (d.type) {
                case 0: G.p.inv.wood += d.amount; break;
                case 1: G.p.inv.metal += d.amount; break;
                case 2: G.p.inv.cloth += d.amount; break;
                case 3: G.p.inv.chem += d.amount; break;
                case 4: G.p.inv.medkits += d.amount; break;
            }
            G.drops.erase(G.drops.begin()+i); continue;
        }
        if (dist < 8.0f && dist > 0.1f) {
            Vector3 dir = Vector3Scale(toP, 1/dist);
            d.pos = Vector3Add(d.pos, Vector3Scale(dir, 5.0f*dt));
        }
    }
}

// ==================== ПОСТРОЙКИ ====================
void updateStructures(float dt) {
    for (int i = (int)G.structures.size()-1; i >= 0; i--) {
        auto& s = G.structures[i];
        if (s.cooldown > 0) s.cooldown -= dt;
        if (s.buildFlash > 0) s.buildFlash -= dt;

        if (s.hp < s.maxHp * 0.5f) {
            s.smokeTimer += dt;
            if (s.smokeTimer > 0.15f) {
                s.smokeTimer = 0;
                Vector3 smokePos = {s.pos.x + rnd(-0.5f, 0.5f), 1.5f, s.pos.z + rnd(-0.5f, 0.5f)};
                G.particles.push_back({smokePos, {rnd(-0.2f,0.2f), rnd(1.0f, 2.0f), rnd(-0.2f,0.2f)},
                                       0.7f, 0.7f, 0.18f, {70,70,70,180}});
            }
        }

        float tierMul = s.level == 1 ? 1.0f : s.level == 2 ? 1.3f : 1.7f;
        float rangeMul = s.level == 1 ? 1.0f : s.level == 2 ? 1.2f : 1.4f;
        bool remove = false;

        if (s.type == 2) {
            int best = -1; float bestD = (20*rangeMul)*(20*rangeMul);
            for (int j = 0; j < (int)G.zombies.size(); j++) {
                float dx = G.zombies[j].pos.x - s.pos.x, dz = G.zombies[j].pos.z - s.pos.z;
                float dd = dx*dx + dz*dz;
                if (dd < bestD) { best = j; bestD = dd; }
            }
            if (best >= 0) {
                Zombie& z = G.zombies[best];
                s.angle = atan2f(z.pos.x - s.pos.x, z.pos.z - s.pos.z);
                if (s.cooldown <= 0) {
                    s.cooldown = 0.5f / (s.level == 3 ? 1.5f : s.level == 2 ? 1.2f : 1.0f);
                    float dmgBoost = 1.0f;
                    for (auto& gen : G.structures) {
                        if (gen.type == 7) {
                            float gx = s.pos.x - gen.pos.x, gz = s.pos.z - gen.pos.z;
                            if (gx*gx + gz*gz < 8*8) { dmgBoost = 1.5f; break; }
                        }
                    }
                    Vector3 d = {z.pos.x - s.pos.x, z.radius*1.5f - s.pos.y, z.pos.z - s.pos.z};
                    float len = Vector3Length(d);
                    if (len > 0.1f) d = Vector3Scale(d, 1.0f/len);
                    Bullet b;
                    b.pos = {s.pos.x, s.pos.y + 0.5f, s.pos.z};
                    b.vel = Vector3Scale(d, 80);
                    b.damage = 15 * dmgBoost * G.p.damageMultiplier * tierMul;
                    b.life = 0.5f; b.radius = 0.08f;
                    b.color = {127,217,255,255};
                    G.bullets.push_back(b);
                }
            }
        }
        else if (s.type == 9) {
            int best = -1; float bestD = (12*rangeMul)*(12*rangeMul);
            for (int j = 0; j < (int)G.zombies.size(); j++) {
                float dx = G.zombies[j].pos.x - s.pos.x, dz = G.zombies[j].pos.z - s.pos.z;
                float dd = dx*dx + dz*dz;
                if (dd < bestD) { best = j; bestD = dd; }
            }
            if (best >= 0) {
                Zombie& z = G.zombies[best];
                s.angle = atan2f(z.pos.x - s.pos.x, z.pos.z - s.pos.z);
                if (s.cooldown <= 0) {
                    s.cooldown = 0.35f;
                    Vector3 d = {z.pos.x - s.pos.x, 0, z.pos.z - s.pos.z};
                    float len = Vector3Length(d);
                    if (len > 0.1f) d = Vector3Scale(d, 1.0f/len);
                    Bullet b;
                    b.pos = {s.pos.x, 0.5f, s.pos.z};
                    b.vel = Vector3Scale(d, 30);
                    b.damage = 8 * tierMul;
                    b.life = 0.5f; b.radius = 0.15f;
                    b.color = {255,120,40,255}; b.explosive = true;
                    G.bullets.push_back(b);
                    burst(b.pos, 3, {255,180,60,255}, 2);
                }
            }
        }
        else if (s.type == 10) {
            int best = -1; float bestD = (30*rangeMul)*(30*rangeMul);
            for (int j = 0; j < (int)G.zombies.size(); j++) {
                float dx = G.zombies[j].pos.x - s.pos.x, dz = G.zombies[j].pos.z - s.pos.z;
                float dd = dx*dx + dz*dz;
                if (dd < bestD) { best = j; bestD = dd; }
            }
            if (best >= 0) {
                Zombie& z = G.zombies[best];
                s.angle = atan2f(z.pos.x - s.pos.x, z.pos.z - s.pos.z);
                if (s.cooldown <= 0) {
                    s.cooldown = 1.5f / tierMul;
                    Vector3 dir = {sinf(s.angle), 0, cosf(s.angle)};
                    for (int j = (int)G.zombies.size()-1; j >= 0; j--) {
                        auto& z2 = G.zombies[j];
                        Vector3 rel = {z2.pos.x - s.pos.x, 0, z2.pos.z - s.pos.z};
                        float projLen = rel.x*dir.x + rel.z*dir.z;
                        if (projLen < 0 || projLen > 30 * rangeMul) continue;
                        Vector3 proj = {dir.x*projLen, 0, dir.z*projLen};
                        float perpDist = Vector3Distance(rel, proj);
                        if (perpDist < 1.0f * rangeMul) {
                            z2.hp -= 80 * G.p.damageMultiplier * tierMul;
                            z2.hitFlash = 0.15f;
                            if (z2.hp <= 0) killZombie(j);
                        }
                    }
                    burst({s.pos.x, 1.5f, s.pos.z}, 15, {200,100,255,255}, 4);
                    G.shake = 3;
                }
            }
        }
        else if (s.type == 3) {
            float rangeMine = 4.0f * rangeMul;
            for (int j = 0; j < (int)G.zombies.size(); j++) {
                float dx = G.zombies[j].pos.x - s.pos.x, dz = G.zombies[j].pos.z - s.pos.z;
                if (dx*dx + dz*dz < rangeMine*rangeMine) {
                    for (int k = (int)G.zombies.size()-1; k >= 0; k--) {
                        float ddx = G.zombies[k].pos.x - s.pos.x, ddz = G.zombies[k].pos.z - s.pos.z;
                        if (ddx*ddx + ddz*ddz < 25.0f * rangeMul) {
                            G.zombies[k].hp -= 100 * G.p.damageMultiplier * tierMul;
                            if (G.zombies[k].hp <= 0) killZombie(k);
                        }
                    }
                    burst(s.pos, 50, {255,155,61,255}, 10);
                    G.shake = 15; remove = true; break;
                }
            }
        }
        else if (s.type == 4) {
            if (s.cooldown <= 0) {
                bool hit = false;
                float elecRange = 16.0f * rangeMul;
                for (int j = (int)G.zombies.size()-1; j >= 0; j--) {
                    float dx = G.zombies[j].pos.x - s.pos.x, dz = G.zombies[j].pos.z - s.pos.z;
                    if (dx*dx + dz*dz < elecRange) {
                        G.zombies[j].hp -= 20 * G.p.damageMultiplier * tierMul;
                        G.zombies[j].hitFlash = 0.1f;
                        hit = true;
                        if (G.zombies[j].hp <= 0) killZombie(j);
                    }
                }
                if (hit) s.cooldown = 0.4f;
            }
        }
        else if (s.type == 5) {
            float spikeR = 1.5f * rangeMul;
            for (int j = (int)G.zombies.size()-1; j >= 0; j--) {
                float dx = G.zombies[j].pos.x - s.pos.x, dz = G.zombies[j].pos.z - s.pos.z;
                float r = spikeR + G.zombies[j].radius;
                if (dx*dx + dz*dz < r*r) {
                    G.zombies[j].hp -= 15 * dt * G.p.damageMultiplier * tierMul;
                    G.zombies[j].speed *= 0.95f;
                    if (G.zombies[j].hp <= 0) killZombie(j);
                }
            }
        }
        else if (s.type == 6) {
            if (s.cooldown <= 0) {
                float dx = G.p.pos.x - s.pos.x, dz = G.p.pos.z - s.pos.z;
                float healR = 6.0f * rangeMul;
                if (dx*dx + dz*dz < healR*healR && G.p.hp < G.p.maxHp) {
                    G.p.hp = std::min(G.p.maxHp, G.p.hp + 10 * tierMul);
                    s.cooldown = 0.5f;
                    burst({G.p.pos.x, 1, G.p.pos.z}, 3, {90,223,122,255}, 2);
                }
            }
        }
        if (remove) G.structures.erase(G.structures.begin() + i);
    }
}

// ==================== ОБЩИЙ UPDATE ====================
void updateGame(float dt) {
    G.time += dt;
    if (G.shake > 0) G.shake -= 30 * dt;
    if (G.bannerTimer > 0) G.bannerTimer -= dt;
    if (G.messageTimer > 0) { G.messageTimer -= dt; if (G.messageTimer <= 0) G.message = ""; }
    if (IsKeyPressed(KEY_TAB)) {
        if (G.state == ST_CRAFT) { G.state = ST_PLAYING; DisableCursor(); }
        else if (G.state == ST_PLAYING) { G.state = ST_CRAFT; EnableCursor(); }
        return;
    }
    if (G.state == ST_CRAFT) return;
    updatePlayer(dt);
    updateZombies(dt);
    updateStructures(dt);
    updateBullets(dt);
    updateParticles(dt);
    updateDrops(dt);
    for (int i = (int)G.bloodDecals.size()-1; i >= 0; i--) {
        G.bloodDecals[i].life -= dt;
        if (G.bloodDecals[i].life <= 0) G.bloodDecals.erase(G.bloodDecals.begin()+i);
    }
    if (!G.spawnQueue.empty()) {
        G.spawnTimer -= dt;
        if (G.spawnTimer <= 0) {
            int type = G.spawnQueue.front();
            G.spawnQueue.erase(G.spawnQueue.begin());
            Zombie z = makeZombie(type, 1.0f + (G.level-1)*0.1f);
            float a = rnd(0, 6.28f);
            z.pos = {G.p.pos.x + cosf(a)*25, 0, G.p.pos.z + sinf(a)*25};
            z.pos.x = clampf(z.pos.x, -WORLD+2, WORLD-2);
            z.pos.z = clampf(z.pos.z, -WORLD+2, WORLD-2);
            G.zombies.push_back(z);
            G.spawnTimer = 0.6f;
        }
    }
    if (G.levelStarted && G.spawnQueue.empty() && G.zombies.empty()) {
        G.levelCompleteTimer += dt;
        if (G.levelCompleteTimer > 1.5f) {
            G.levelCompleteTimer = 0; G.levelStarted = false;
            G.state = ST_LEVEL_COMPLETE;
        }
    } else G.levelCompleteTimer = 0;
}

// ==================== ДЕТАЛИЗИРОВАННЫЕ ПОСТРОЙКИ ====================
void drawStructureDetailed(const Structure& s) {
    const BuildTypeDef& bt = BUILD_TYPES[s.type];
    float sz = bt.size * 2.0f;
    float hh = bt.size * 2.0f;
    float cx = s.pos.x, cz = s.pos.z;
    Color col = bt.color;
    Color darkC = {(unsigned char)(col.r*0.55f), (unsigned char)(col.g*0.55f), (unsigned char)(col.b*0.55f), 255};
    Color lightC = {(unsigned char)std::min(255, (int)(col.r*1.25f)), (unsigned char)std::min(255, (int)(col.g*1.25f)), (unsigned char)std::min(255, (int)(col.b*1.25f)), 255};

    int lvl = s.level;
    if (lvl >= 2) {
        if (lvl == 3) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere({cx, hh*0.5f, cz}, hh*1.3f, {220, 180, 60, 25});
            EndBlendMode();
        }
    }

    DrawCube({cx, 0.1f, cz}, sz * 1.05f, 0.2f, sz * 1.05f, {50, 45, 40, 255});
    DrawCubeWires({cx, 0.1f, cz}, sz * 1.05f, 0.2f, sz * 1.05f, {30, 25, 20, 255});
    DrawCylinderEx({cx, 0.02f, cz}, {cx, 0.03f, cz}, sz*0.9f, sz*0.9f, 16, {0,0,0,80});

    switch (s.type) {
        case 0: {
            for (int i = 0; i < 5; i++) {
                float y = 0.3f + i * (hh - 0.4f) / 5.0f;
                float shade = 1.0f - i * 0.05f;
                Color pc = {(unsigned char)(col.r*shade), (unsigned char)(col.g*shade), (unsigned char)(col.b*shade), 255};
                DrawCube({cx, y, cz}, sz, (hh-0.4f)/5.0f * 0.92f, sz, pc);
                DrawCubeWires({cx, y, cz}, sz, (hh-0.4f)/5.0f * 0.92f, sz, darkC);
                DrawSphere({cx - sz/2 + 0.1f, y, cz - sz/2 + 0.1f}, 0.04f, darkC);
                DrawSphere({cx + sz/2 - 0.1f, y, cz - sz/2 + 0.1f}, 0.04f, darkC);
                DrawSphere({cx - sz/2 + 0.1f, y, cz + sz/2 - 0.1f}, 0.04f, darkC);
                DrawSphere({cx + sz/2 - 0.1f, y, cz + sz/2 - 0.1f}, 0.04f, darkC);
            }
            DrawCylinder({cx-sz/2+0.1f, 0, cz-sz/2+0.1f}, 0.08f, 0.1f, hh, 6, darkC);
            DrawCylinder({cx+sz/2-0.1f, 0, cz-sz/2+0.1f}, 0.08f, 0.1f, hh, 6, darkC);
            DrawCylinder({cx-sz/2+0.1f, 0, cz+sz/2-0.1f}, 0.08f, 0.1f, hh, 6, darkC);
            DrawCylinder({cx+sz/2-0.1f, 0, cz+sz/2-0.1f}, 0.08f, 0.1f, hh, 6, darkC);
            if (lvl >= 2) {
                Color metalA = {180, 180, 190, 255};
                for (int s2 = -1; s2 <= 1; s2 += 2) for (int s3 = -1; s3 <= 1; s3 += 2) {
                    DrawCube({cx + s2*(sz/2-0.15f), hh*0.5f, cz + s3*(sz/2-0.15f)}, 0.15f, hh*0.7f, 0.15f, metalA);
                }
            }
            if (lvl == 3) {
                DrawCylinder({cx, hh, cz}, 0.03f, 0.03f, 1.2f, 4, darkC);
                DrawCube({cx + 0.4f, hh + 0.8f, cz}, 0.8f, 0.5f, 0.02f, {180, 50, 50, 255});
            }
            break;
        }
        case 1: {
            DrawCube({cx, hh/2, cz}, sz, hh, sz, col);
            DrawCubeWires({cx, hh/2, cz}, sz, hh, sz, darkC);
            for (int i = 0; i < 3; i++) {
                float px = cx - sz/2 + (i+1) * sz/4.0f;
                DrawCube({px, hh/2, cz}, 0.06f, hh, sz*1.05f, darkC);
            }
            for (int yy = 0; yy < 3; yy++) for (int xx = 0; xx < 3; xx++) {
                float py = 0.3f + yy * (hh-0.6f)/2.0f;
                float px = cx - sz/2 + 0.2f + xx * (sz-0.4f)/2.0f;
                DrawSphere({px, py, cz + sz/2 + 0.01f}, 0.05f, lightC);
                DrawSphere({px, py, cz - sz/2 - 0.01f}, 0.05f, lightC);
            }
            DrawCube({cx, hh + 0.05f, cz}, sz*1.1f, 0.1f, sz*1.1f, lightC);
            if (lvl >= 2) {
                for (int i = 0; i < 3; i++) {
                    DrawCube({cx, 0.15f + i * 0.1f, cz + sz/2 + 0.02f}, sz*0.9f, 0.08f, 0.02f, {220, 200, 60, 255});
                }
            }
            if (lvl == 3) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawCubeWires({cx, hh/2, cz}, sz*1.05f, hh*1.05f, sz*1.05f, {100, 200, 255, 180});
                EndBlendMode();
            }
            break;
        }
        case 2: {
            DrawCylinder({cx, 0.2f, cz}, sz*0.55f, sz*0.6f, 0.5f, 12, darkC);
            DrawCylinderWires({cx, 0.2f, cz}, sz*0.55f, sz*0.6f, 0.5f, 12, BLACK);
            DrawCylinder({cx, 0.7f, cz}, sz*0.45f, sz*0.35f, 0.6f, 12, col);
            DrawCylinderWires({cx, 0.7f, cz}, sz*0.45f, sz*0.35f, 0.6f, 12, BLACK);
            DrawSphere({cx, 1.4f, cz}, 0.35f, col);
            DrawSphereWires({cx, 1.4f, cz}, 0.35f, 8, 8, BLACK);
            Vector3 m = {cx, 1.45f, cz};
            Vector3 t = {cx + sinf(s.angle)*1.3f, 1.45f, cz + cosf(s.angle)*1.3f};
            DrawCylinderEx(m, t, 0.12f, 0.09f, 8, darkC);
            DrawCylinderWiresEx(m, t, 0.13f, 0.10f, 8, BLACK);
            Vector3 tip = {t.x + sinf(s.angle)*0.15f, t.y, t.z + cosf(s.angle)*0.15f};
            DrawCylinderEx(t, tip, 0.14f, 0.14f, 8, {30,35,40,255});
            float blink = (sinf(G.time*8) + 1) * 0.5f;
            DrawSphere({cx + sinf(s.angle)*0.35f, 1.6f, cz + cosf(s.angle)*0.35f}, 0.08f, {255, (unsigned char)(50+blink*100), 50, 255});
            if (s.cooldown > 0.35f) DrawSphere(tip, 0.25f, {255,220,100,255});
            if (lvl >= 2) {
                Vector3 m2 = {cx - sinf(s.angle)*0.05f, 1.45f, cz - cosf(s.angle)*0.05f};
                Vector3 t2 = {t.x - sinf(s.angle)*0.05f, 1.45f, t.z - cosf(s.angle)*0.05f};
                DrawCylinderEx(m2, t2, 0.08f, 0.06f, 6, darkC);
            }
            if (lvl == 3) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawCylinderWires({cx, 0.7f, cz}, sz*0.5f, sz*0.4f, 0.6f, 12, {100, 200, 255, 180});
                DrawSphere(tip, 0.3f, {100, 200, 255, 80});
                EndBlendMode();
            }
            break;
        }
        case 3: {
            DrawCylinder({cx, 0.15f, cz}, 0.5f, 0.5f, 0.1f, 12, {60,60,60,255});
            DrawCylinderWires({cx, 0.15f, cz}, 0.5f, 0.5f, 0.1f, 12, BLACK);
            DrawSphere({cx, 0.25f, cz}, 0.25f, darkC);
            float bl = (sinf(G.time*8) + 1) * 0.5f;
            DrawSphere({cx, 0.45f, cz}, 0.08f, {255, (unsigned char)(80+bl*175), 80, 255});
            DrawCylinderEx({cx, 0.25f, cz}, {cx, 0.6f, cz}, 0.02f, 0.02f, 4, {150,150,150,255});
            float radius = 5.0f * (s.level == 1 ? 1.0f : s.level == 2 ? 1.2f : 1.4f);
            DrawCylinderWires({cx, 0.02f, cz}, radius, radius, 0.02f, 24, {255,80,80,60});
            if (lvl >= 2) {
                for (int i = 0; i < 4; i++) {
                    float a = (i/4.0f)*6.28f;
                    DrawSphere({cx + cosf(a)*0.35f, 0.25f, cz + sinf(a)*0.35f}, 0.04f, {255, 150, 50, 255});
                }
            }
            if (lvl == 3) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawSphere({cx, 0.4f, cz}, 0.3f, {255, 150, 50, 120});
                EndBlendMode();
            }
            break;
        }
        case 4: {
            Vector3 corners[4] = {
                {cx - sz/2 + 0.1f, 0.3f, cz - sz/2 + 0.1f},
                {cx + sz/2 - 0.1f, 0.3f, cz - sz/2 + 0.1f},
                {cx + sz/2 - 0.1f, 0.3f, cz + sz/2 - 0.1f},
                {cx - sz/2 + 0.1f, 0.3f, cz + sz/2 - 0.1f}
            };
            for (int i = 0; i < 4; i++) {
                DrawCylinder(corners[i], 0.1f, 0.12f, hh * 0.9f, 6, darkC);
                DrawCylinderWires(corners[i], 0.1f, 0.12f, hh * 0.9f, 6, BLACK);
                DrawSphere({corners[i].x, hh * 0.9f + 0.4f, corners[i].z}, 0.15f, {200,80,80,255});
            }
            float sparkPhase = G.time * 15;
            for (int i = 0; i < 4; i++) {
                int j = (i + 1) % 4;
                Vector3 a = {corners[i].x, hh*0.9f + 0.3f, corners[i].z};
                Vector3 b = {corners[j].x, hh*0.9f + 0.3f, corners[j].z};
                DrawCylinderEx(a, b, 0.03f, 0.03f, 4, {200,220,255,200});
                for (int k = 0; k < 3; k++) {
                    float t = fmodf(sparkPhase + k * 0.33f, 1.0f);
                    Vector3 sp = {a.x + (b.x-a.x)*t, a.y + rnd(-0.05f,0.05f), a.z + (b.z-a.z)*t};
                    DrawSphere(sp, 0.06f, {150,220,255,255});
                }
            }
            float pulse = (sinf(G.time*4) + 1) * 0.5f;
            Color fieldC = {127, 217, 255, (unsigned char)(40 + pulse*60)};
            float fieldR = sz*0.65f * (s.level == 1 ? 1.0f : 1.15f);
            DrawCylinder({cx, 0.5f, cz}, fieldR, fieldR, hh*0.9f, 16, fieldC);
            if (lvl == 3) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawCylinderWires({cx, 0.2f, cz}, fieldR*1.1f, fieldR*1.1f, hh, 16, {200, 240, 255, 200});
                EndBlendMode();
            }
            break;
        }
        case 5: {
            DrawCube({cx, 0.25f, cz}, sz, 0.3f, sz, {80,70,60,255});
            DrawCubeWires({cx, 0.25f, cz}, sz, 0.3f, sz, BLACK);
            int spikeGrid = s.level == 1 ? 3 : 4;
            for (int xi = 0; xi < spikeGrid; xi++) for (int zi = 0; zi < spikeGrid; zi++) {
                float px = cx - sz/2 + (xi+0.5f) * sz/spikeGrid;
                float pz = cz - sz/2 + (zi+0.5f) * sz/spikeGrid;
                float spikeH = 0.8f * (s.level == 1 ? 1.0f : s.level == 2 ? 1.25f : 1.5f);
                DrawCylinder({px, 0.4f, pz}, 0.12f, 0.02f, spikeH, 5, darkC);
                DrawCylinder({px, 0.4f + spikeH, pz}, 0.02f, 0.0f, 0.2f, 5, {220,220,220,255});
                DrawSphere({px, 0.4f + spikeH + 0.15f, pz}, 0.03f, {150,20,20,255});
            }
            if (lvl == 3) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawSphere({cx, 0.8f, cz}, sz*0.7f, {255, 50, 50, 40});
                EndBlendMode();
            }
            break;
        }
        case 6: {
            DrawCylinder({cx, 0.2f, cz}, sz*0.5f, sz*0.4f, 0.8f, 12, darkC);
            DrawCylinderWires({cx, 0.2f, cz}, sz*0.5f, sz*0.4f, 0.8f, 12, BLACK);
            DrawCylinder({cx, 1.0f, cz}, 0.15f, 0.12f, 1.0f, 8, col);
            float glow = 0.6f + (sinf(G.time*3) + 1) * 0.2f;
            Color crossC = {90, 223, 122, (unsigned char)(255*glow)};
            DrawCube({cx, 2.2f, cz}, 0.7f, 0.15f, 0.15f, crossC);
            DrawCube({cx, 2.2f, cz}, 0.15f, 0.7f, 0.15f, crossC);
            float auraR = 1.5f * (s.level == 1 ? 1.0f : s.level == 2 ? 1.3f : 1.7f);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere({cx, 2.2f, cz}, auraR + sinf(G.time*2)*0.2f, {90, 223, 122, 30});
            if (lvl >= 2) DrawCube({cx, 2.2f, cz}, 0.15f, 0.15f, 0.7f, crossC);
            if (lvl == 3) DrawSphere({cx, 2.2f, cz}, 0.5f, {150, 255, 180, 80});
            EndBlendMode();
            break;
        }
        case 7: {
            DrawCube({cx, 0.8f, cz}, sz*0.9f, 1.2f, sz*0.9f, darkC);
            DrawCubeWires({cx, 0.8f, cz}, sz*0.9f, 1.2f, sz*0.9f, BLACK);
            for (int i = 0; i < 3; i++) {
                float px = cx - 0.3f + i * 0.3f;
                DrawCylinder({px, 0.35f, cz}, 0.15f, 0.15f, 0.9f, 8, {184, 115, 51, 255});
                DrawCylinderWires({px, 0.35f, cz}, 0.15f, 0.15f, 0.9f, 8, BLACK);
            }
            DrawCylinder({cx, 1.45f, cz}, 0.3f, 0.3f, 0.1f, 8, {200,180,60,255});
            for (int i = 0; i < 8; i++) {
                float a = (i/8.0f)*6.28f;
                DrawCube({cx + cosf(a)*0.35f, 1.55f, cz + sinf(a)*0.35f}, 0.1f, 0.15f, 0.1f, darkC);
            }
            if (chance(0.15f)) {
                float a = rnd(0, 6.28f);
                DrawLine3D({cx, 1.55f, cz}, {cx + cosf(a)*0.5f, 1.55f + rnd(0.1f, 0.5f), cz + sinf(a)*0.5f}, {200,220,255,255});
            }
            float bl = (sinf(G.time*6) + 1) * 0.5f;
            DrawSphere({cx + 0.3f, 1.2f, cz + 0.3f}, 0.08f, {255, (unsigned char)(150+bl*100), 50, 255});
            if (lvl == 3) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawSphere({cx, 0.8f, cz}, 2.0f, {255, 220, 80, 30});
                EndBlendMode();
            }
            break;
        }
        case 8: {
            for (int i = 0; i < 4 + lvl; i++) {
                float y = 0.3f + i * 0.35f;
                float tilt = (i % 2 == 0) ? 0.15f : -0.15f;
                Vector3 a = {cx - sz*0.7f, y, cz};
                Vector3 b = {cx + sz*0.7f, y + tilt, cz};
                DrawCylinderEx(a, b, 0.1f, 0.1f, 6, col);
            }
            for (int s2 = -1; s2 <= 1; s2 += 2) {
                for (int k = 0; k < lvl; k++) {
                    DrawSphere({cx + s2*sz*0.6f, 0.3f + k*0.3f, cz}, 0.4f, {150, 130, 80, 255});
                }
            }
            if (lvl >= 2) DrawCube({cx, hh*0.5f, cz - sz*0.3f}, sz*1.5f, 0.1f, 0.15f, darkC);
            break;
        }
        case 9: {
            DrawCylinder({cx, 0.3f, cz}, 0.5f, 0.5f, 1.2f, 12, {120, 50, 20, 255});
            DrawCylinderWires({cx, 0.3f, cz}, 0.5f, 0.5f, 1.2f, 12, BLACK);
            Vector3 m = {cx, 1.5f, cz};
            Vector3 t = {cx + sinf(s.angle)*1.1f, 1.5f, cz + cosf(s.angle)*1.1f};
            DrawCylinderEx(m, t, 0.15f, 0.2f, 8, darkC);
            float pulse = (sinf(G.time*10) + 1) * 0.5f;
            Vector3 tip = {t.x + sinf(s.angle)*0.4f, t.y, t.z + cosf(s.angle)*0.4f};
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere(tip, 0.3f + pulse*0.15f, {255, (unsigned char)(120+pulse*100), 40, 200});
            DrawSphere({tip.x + sinf(s.angle)*0.3f, tip.y, tip.z + cosf(s.angle)*0.3f}, 0.15f + pulse*0.1f, {255, 200, 80, 120});
            if (lvl >= 2) {
                Vector3 t2 = {t.x + cosf(s.angle)*0.15f, t.y, t.z - sinf(s.angle)*0.15f};
                DrawSphere(t2, 0.2f + pulse*0.1f, {255, 100, 30, 150});
            }
            EndBlendMode();
            DrawSphere({cx + 0.3f, 0.8f, cz + 0.3f}, 0.1f, {255, 100, 50, 255});
            break;
        }
        case 10: {
            DrawCylinder({cx, 0.2f, cz}, 0.6f, 0.5f, 0.4f, 12, darkC);
            DrawCylinderWires({cx, 0.2f, cz}, 0.6f, 0.5f, 0.4f, 12, BLACK);
            DrawSphere({cx, 0.9f, cz}, 0.35f, {140, 40, 180, 255});
            DrawSphereWires({cx, 0.9f, cz}, 0.36f, 10, 10, {200, 100, 255, 255});
            Vector3 m = {cx, 0.9f, cz};
            Vector3 t = {cx + sinf(s.angle)*1.2f, 0.9f, cz + cosf(s.angle)*1.2f};
            DrawCylinderEx(m, t, 0.15f, 0.1f, 8, {100, 30, 140, 255});
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere(t, 0.3f + sinf(G.time*6)*0.05f, {200, 100, 255, 100});
            if (s.cooldown > 1.3f) {
                DrawLine3D(t, {t.x + sinf(s.angle)*10, t.y, t.z + cosf(s.angle)*10}, {255, 150, 255, 220});
            }
            if (lvl >= 2) DrawSphere({cx, 0.9f, cz}, 0.5f, {200, 100, 255, 60});
            EndBlendMode();
            break;
        }
        case 11: {
            DrawCylinder({cx, 0.2f, cz}, 0.1f, 0.08f, 1.6f, 6, {80, 50, 30, 255});
            DrawCylinder({cx, 1.8f, cz}, 0.25f, 0.15f, 0.15f, 8, darkC);
            float pulse = (sinf(G.time*8) + 1) * 0.5f;
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere({cx, 2.05f + pulse*0.1f, cz}, 0.3f + pulse*0.1f, {255, 180, 60, 200});
            DrawSphere({cx, 2.15f + pulse*0.15f, cz}, 0.15f + pulse*0.05f, {255, 240, 150, 180});
            DrawSphere({cx, 2.05f, cz}, 3.0f, {255, 200, 100, 20});
            if (lvl >= 2) DrawSphere({cx, 2.05f, cz}, 4.5f, {255, 200, 100, 15});
            EndBlendMode();
            break;
        }
    }

    if (s.hp < s.maxHp) {
        float pct = s.hp / s.maxHp;
        Color barCol = pct > 0.5f ? GREEN : (pct > 0.25f ? ORANGE : RED);
        DrawCube({cx - sz/2 + sz*pct/2, hh + 0.5f, cz}, sz*pct, 0.08f, 0.1f, barCol);
        DrawCubeWires({cx, hh + 0.5f, cz}, sz, 0.1f, 0.12f, BLACK);
    }

    for (int i = 0; i < lvl; i++) {
        float starX = cx - 0.3f + i * 0.3f;
        DrawSphere({starX, hh + 0.8f, cz}, 0.1f, GOLD);
    }

    if (s.buildFlash > 0) {
        float a = s.buildFlash / 0.6f;
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphere({cx, hh*0.5f, cz}, hh*1.5f, {255, 240, 180, (unsigned char)(150*a)});
        EndBlendMode();
    }
}

// ==================== ОРУЖИЕ ====================
void drawWeapon3D() {
    Player& pl = G.p;
    Vector3 eye = {pl.pos.x, EYE_H, pl.pos.z};
    Vector3 fwd = ForwardDir(pl.yaw, pl.pitch);
    Vector3 rightDir = Vector3Normalize(Vector3{fwd.z, 0, -fwd.x});
    Vector3 up = {0, 1, 0};
    float shakeX = pl.shakeRecoilX * pl.shakeRecoil * 0.35f;
    float shakeY = pl.shakeRecoilY * pl.shakeRecoil * 0.35f;
    float bobX = sinf(pl.bobPhase) * pl.bobAmount * 0.025f;
    float bobY = fabs(sinf(pl.bobPhase * 2.0f)) * pl.bobAmount * 0.02f;
    Vector3 gunStart = eye;
    gunStart = Vector3Add(gunStart, Vector3Scale(rightDir, 0.22f + shakeX + bobX));
    gunStart = Vector3Add(gunStart, Vector3Scale(up, -0.22f - pl.recoil*0.5f + shakeY - bobY));
    float gunRadius = 0.06f, gunLen = 0.6f;
    Color gunColor = {60,60,70,255};
    if (pl.currentWeapon == W_SHOTGUN)      { gunRadius = 0.09f; gunLen = 0.6f;  gunColor = {80,60,50,255}; }
    else if (pl.currentWeapon == W_RIFLE)   { gunRadius = 0.05f; gunLen = 0.9f;  gunColor = {60,60,80,255}; }
    else if (pl.currentWeapon == W_SMG)     { gunRadius = 0.05f; gunLen = 0.5f;  gunColor = {70,70,60,255}; }
    else if (pl.currentWeapon == W_FLAME)   { gunRadius = 0.10f; gunLen = 0.55f; gunColor = {150,50,30,255}; }
    else                                    { gunRadius = 0.05f; gunLen = 0.35f; gunColor = {60,60,70,255}; }
    Vector3 gunEnd = Vector3Add(gunStart, Vector3Scale(fwd, gunLen));
    gunEnd = Vector3Add(gunEnd, Vector3Scale(rightDir, shakeX * 1.2f));
    gunEnd = Vector3Add(gunEnd, Vector3Scale(up, shakeY * 1.2f));
    DrawCylinderEx(gunStart, gunEnd, gunRadius, gunRadius * 0.85f, 10, gunColor);
    DrawCylinderWiresEx(gunStart, gunEnd, gunRadius*1.02f, gunRadius*0.88f, 10, (Color){20,20,25,255});
    Vector3 gripStart = Vector3Add(gunStart, Vector3Scale(up, -gunRadius*0.5f));
    Vector3 gripEnd = Vector3Add(gripStart, Vector3Scale(up, -0.12f));
    gripEnd = Vector3Add(gripEnd, Vector3Scale(fwd, -0.03f));
    DrawCylinderEx(gripStart, gripEnd, gunRadius*0.7f, gunRadius*0.6f, 6, (Color){40,30,20,255});
    if (pl.currentWeapon == W_RIFLE || pl.currentWeapon == W_SHOTGUN) {
        Vector3 stockStart = Vector3Add(gunStart, Vector3Scale(fwd, -0.05f));
        Vector3 stockEnd = Vector3Add(stockStart, Vector3Scale(fwd, -0.25f));
        stockEnd = Vector3Add(stockEnd, Vector3Scale(up, -0.03f));
        DrawCylinderEx(stockStart, stockEnd, gunRadius*1.2f, gunRadius*1.5f, 6, (Color){50,35,25,255});
    }
    Vector3 scopeBase = Vector3Add(gunStart, Vector3Scale(fwd, gunLen * 0.5f));
    scopeBase = Vector3Add(scopeBase, Vector3Scale(up, gunRadius + 0.03f));
    DrawCube(scopeBase, 0.05f, 0.05f, gunLen*0.3f, (Color){30,30,35,255});
    if (pl.flash > 0) {
        float fs = pl.flash / 0.05f;
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphere(gunEnd, 2.5f * fs, {255, 200, 120, (unsigned char)(60 * fs)});
        DrawSphere(gunEnd, 1.2f * fs, {255, 220, 150, (unsigned char)(120 * fs)});
        DrawSphere(gunEnd, 0.25f, {255,220,100,200});
        DrawSphere(gunEnd, 0.15f, {255,255,200,255});
        DrawSphere(gunEnd, 0.08f, WHITE);
        EndBlendMode();
    }
}

// ==================== ДОЖДЬ ====================
void drawRain() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    for (int i = 0; i < 180; i++) {
        float seed = i * 13.73f;
        float speed = 600 + fmodf(seed * 71.3f, 400);
        float x = fmodf(seed * 137.5f + G.time * 100, (float)sw);
        float y = fmodf(i * 71.3f + G.time * speed, (float)sh * 1.2f) - (float)sh * 0.2f;
        float len = 15 + fmodf(seed * 31.7f, 15);
        DrawLine((int)x, (int)y, (int)(x - len * 0.3f), (int)(y + len), Color{150, 180, 220, 70});
    }
    DrawRectangle(0, 0, sw, sh, Color{30, 40, 60, 15});
}

// ==================== 3D МИР ====================
void drawWorld3D() {
    DrawPlane({0, 0, 0}, {WORLD*2, WORLD*2}, {30,35,25,255});
    DrawGrid(80, 2.5f);
    BeginBlendMode(BLEND_ALPHA);
    for (int i = 0; i < 8; i++) {
        float r = WORLD * 0.15f + i * WORLD * 0.12f;
        float alpha = 8 + i * 4;
        DrawCylinderEx({G.p.pos.x, 0.01f, G.p.pos.z}, {G.p.pos.x, 0.02f, G.p.pos.z}, r, r, 32, {20, 25, 35, (unsigned char)alpha});
    }
    EndBlendMode();
    for (auto& bd : G.bloodDecals) {
        float a = std::min(1.0f, bd.life / 5.0f);
        DrawCylinderEx({bd.pos.x, 0.02f, bd.pos.z}, {bd.pos.x, 0.03f, bd.pos.z}, bd.size, bd.size, 12, {110, 15, 15, (unsigned char)(190 * a)});
    }
    for (auto& e : G.env) {
        if (e.type == 0) {
            DrawCylinder({e.pos.x, 0, e.pos.z}, 0.2f, 0.3f, e.size, 6, {80,50,30,255});
            DrawCylinder({e.pos.x, e.size, e.pos.z}, 0.15f, 0.2f, e.size*0.3f, 6, {70,45,28,255});
            DrawSphere({e.pos.x, e.size, e.pos.z}, e.size*0.7f, {40,80,40,255});
            DrawSphere({e.pos.x - 0.3f, e.size*0.9f, e.pos.z}, e.size*0.5f, {50,95,50,255});
            DrawSphere({e.pos.x + 0.2f, e.size*1.1f, e.pos.z + 0.2f}, e.size*0.4f, {45,85,45,255});
        } else if (e.type == 1) {
            DrawCube({e.pos.x, e.size*0.4f, e.pos.z}, e.size, e.size*0.8f, e.size, {90,90,90,255});
            DrawCube({e.pos.x + 0.2f, e.size*0.7f, e.pos.z - 0.1f}, e.size*0.7f, e.size*0.5f, e.size*0.7f, {100,100,100,255});
        } else if (e.type == 2) {
            DrawCube({e.pos.x, e.size*0.5f, e.pos.z}, e.size*1.5f, e.size, e.size*1.5f, {80,70,60,255});
            DrawCube({e.pos.x, e.size*1.3f, e.pos.z}, e.size*0.8f, e.size*0.6f, e.size*0.8f, {70,60,50,255});
            DrawCubeWires({e.pos.x, e.size*0.5f, e.pos.z}, e.size*1.5f, e.size, e.size*1.5f, {40,35,30,255});
        }
    }
    for (auto& s : G.structures) drawStructureDetailed(s);
    for (auto& d : G.drops) {
        Color c;
        switch (d.type) {
            case 0: c = {160,122,69,255}; break;
            case 1: c = {138,153,168,255}; break;
            case 2: c = {200,100,200,255}; break;
            case 3: c = {200,200,50,255}; break;
            case 4: c = {90,223,122,255}; break;
            default: c = WHITE;
        }
        float bob = sinf(d.bob) * 0.15f;
        DrawCube({d.pos.x, 0.5f + bob, d.pos.z}, 0.4f, 0.4f, 0.4f, c);
        DrawCubeWires({d.pos.x, 0.5f + bob, d.pos.z}, 0.4f, 0.4f, 0.4f, BLACK);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphere({d.pos.x, 0.5f + bob, d.pos.z}, 0.5f, (Color){c.r, c.g, c.b, 40});
        EndBlendMode();
    }
    for (auto& z : G.zombies) {
        Color bodyColor = z.hitFlash > 0 ? WHITE : z.color;
        if (z.burnTimer > 0) {
            float bp = (sinf(G.time*20 + z.wobble) + 1) * 0.5f;
            bodyColor = {(unsigned char)(200+bp*55), (unsigned char)(100+bp*80), 30, 255};
        }
        Color darkColor = {(unsigned char)(z.color.r/2), (unsigned char)(z.color.g/2), (unsigned char)(z.color.b/2), 255};
        Vector3 faceDir = Vector3Normalize({G.p.pos.x - z.pos.x, 0, G.p.pos.z - z.pos.z});
        Vector3 rightV = {faceDir.z, 0, -faceDir.x};
        float h = z.radius * 2.5f * z.scale;
        float legH = h * 0.35f, torsoH = h * 0.65f;
        float legOff = z.radius * 0.4f * z.scale;
        DrawCylinderEx({z.pos.x, 0.02f, z.pos.z}, {z.pos.x, 0.03f, z.pos.z}, z.radius*1.4f*z.scale, z.radius*1.4f*z.scale, 14, {0,0,0,120});
        DrawCylinder({z.pos.x - rightV.x*legOff, 0, z.pos.z - rightV.z*legOff}, z.radius*0.25f*z.scale, z.radius*0.3f*z.scale, legH, 6, darkColor);
        DrawCylinder({z.pos.x + rightV.x*legOff, 0, z.pos.z + rightV.z*legOff}, z.radius*0.25f*z.scale, z.radius*0.3f*z.scale, legH, 6, darkColor);
        DrawCylinder({z.pos.x, legH, z.pos.z}, z.radius*0.85f*z.scale, z.radius*0.7f*z.scale, torsoH, 10, bodyColor);
        DrawCylinderWires({z.pos.x, legH, z.pos.z}, z.radius*0.85f*z.scale, z.radius*0.7f*z.scale, torsoH, 10, darkColor);
        float headY = legH + torsoH + z.radius*0.5f*z.scale;
        DrawSphere({z.pos.x, headY, z.pos.z}, z.radius*0.65f*z.scale, bodyColor);
        float armLen = h * 0.5f;
        for (int side = -1; side <= 1; side += 2) {
            Vector3 sh = {z.pos.x + rightV.x*side*z.radius*0.7f*z.scale, legH + torsoH*0.85f, z.pos.z + rightV.z*side*z.radius*0.7f*z.scale};
            Vector3 hd = {sh.x + faceDir.x*armLen, sh.y + 0.05f*sinf(G.time*4 + z.wobble + side), sh.z + faceDir.z*armLen};
            DrawCylinderEx(sh, hd, z.radius*0.22f*z.scale, z.radius*0.16f*z.scale, 6, bodyColor);
            DrawSphere(hd, z.radius*0.18f*z.scale, darkColor);
        }
        Color eyeColor = z.isBoss ? (Color){255,48,48,255} : (Color){255,80,80,255};
        Vector3 eb = {z.pos.x + faceDir.x*z.radius*0.4f*z.scale, headY + z.radius*0.05f*z.scale, z.pos.z + faceDir.z*z.radius*0.4f*z.scale};
        DrawSphere({eb.x + rightV.x*0.15f*z.scale, eb.y, eb.z + rightV.z*0.15f*z.scale}, 0.08f*z.scale, eyeColor);
        DrawSphere({eb.x - rightV.x*0.15f*z.scale, eb.y, eb.z - rightV.z*0.15f*z.scale}, 0.08f*z.scale, eyeColor);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphere({eb.x, eb.y, eb.z}, 0.15f*z.scale, (Color){eyeColor.r, eyeColor.g, eyeColor.b, 60});
        EndBlendMode();
        if (z.isBoss) {
            float pulse = 1.0f + sinf(G.time*4)*0.15f;
            Color auraC = z.phase == 0 ? (Color){255,200,50,220} : z.phase == 1 ? (Color){255,100,50,220} : (Color){255,30,30,220};
            DrawCylinderWires({z.pos.x, 0.05f, z.pos.z}, (z.radius + 1.0f)*pulse*z.scale, (z.radius + 1.0f)*pulse*z.scale, 0.1f, 16, auraC);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere({z.pos.x, h*0.5f, z.pos.z}, (z.radius + 1.5f)*pulse, (Color){auraC.r, auraC.g, auraC.b, 30});
            EndBlendMode();
        }
        if (z.isBoss && z.castType == 1) {
            float prog = 1.0f - z.castTimer / z.castMax;
            DrawCylinderWires({z.castPos.x, 0.02f, z.castPos.z}, 5.0f, 5.0f, 0.05f, 32, {255,48,96,200});
            DrawCylinderWires({z.castPos.x, 0.03f, z.castPos.z}, 5.0f*prog, 5.0f*prog, 0.06f, 32, {255,200,100,255});
        }
    }
    for (auto& b : G.bullets) {
        DrawSphere(b.pos, b.radius, b.color);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphere(b.pos, b.radius*2.5f, (Color){b.color.r, b.color.g, b.color.b, 60});
        EndBlendMode();
        if (b.hostile) DrawSphereWires(b.pos, b.radius*1.5f, 6, 6, WHITE);
    }
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto& p : G.particles) {
        float a = p.life / p.maxLife;
        Color c = p.color; c.a = (unsigned char)(255 * a);
        DrawSphere(p.pos, p.size * a * 1.3f, c);
    }
    EndBlendMode();
    drawWeapon3D();
    if (G.buildMode) {
        const BuildTypeDef& bt = BUILD_TYPES[G.buildType];
        float sz = bt.size * 2.0f, hh = bt.size * 2.0f;
        Color ghostC = G.buildValid ? (Color){50,200,50,80} : (Color){200,50,50,80};
        DrawCube({G.buildPos.x, hh/2, G.buildPos.z}, sz, hh, sz, ghostC);
        DrawCubeWires({G.buildPos.x, hh/2, G.buildPos.z}, sz, hh, sz, G.buildValid ? GREEN : RED);
        float pulse = (sinf(G.time*6) + 1) * 0.5f;
        DrawCubeWires({G.buildPos.x, hh/2, G.buildPos.z}, sz*(1 + pulse*0.1f), hh*(1 + pulse*0.1f), sz*(1 + pulse*0.1f),
                      (Color){(unsigned char)(G.buildValid ? 90 : 255), (unsigned char)(G.buildValid ? 223 : 50), 122, (unsigned char)(100*(1-pulse))});
    }
}

// ==================== МИНИКАРТА ====================
void drawMinimap() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int mw = 180, mh = 180;
    int mx = sw - mw - 20, my = sh - mh - 20;
    DrawRectangle(mx, my, mw, mh, Color{0, 0, 10, 200});
    DrawRectangleLines(mx, my, mw, mh, Color{100, 150, 200, 200});
    float scale = mw / 100.0f;
    float px = mx + mw / 2.0f, py = my + mh / 2.0f;
    for (auto& z : G.zombies) {
        float rx = px + (z.pos.x - G.p.pos.x) * scale;
        float ry = py + (z.pos.z - G.p.pos.z) * scale;
        if (rx < mx+2 || rx > mx+mw-2 || ry < my+2 || ry > my+mh-2) continue;
        Color c = z.isBoss ? RED : (z.type == Z_BRUTE ? MAGENTA : (z.type == Z_RUNNER ? ORANGE : GREEN));
        DrawCircle((int)rx, (int)ry, z.isBoss ? 4 : 2, c);
    }
    for (auto& s : G.structures) {
        float rx = px + (s.pos.x - G.p.pos.x) * scale;
        float ry = py + (s.pos.z - G.p.pos.z) * scale;
        if (rx < mx+2 || rx > mx+mw-2 || ry < my+2 || ry > my+mh-2) continue;
        // ИСПРАВЛЕНО: SKYBLUE вместо CYAN
        DrawRectangle((int)rx-1, (int)ry-1, 3, 3, s.level == 3 ? GOLD : (s.level == 2 ? ORANGE : SKYBLUE));
    }
    DrawCircle((int)px, (int)py, 3, BLUE);
    DrawLine((int)px, (int)py, (int)(px + cosf(G.p.yaw)*8), (int)(py + sinf(G.p.yaw)*8), WHITE);
    // ИСПРАВЛЕНО: приведение к float
    DrawTextEx(gFont, "MINIMAP", {(float)(mx + 5), (float)(my + 5)}, 12, 1, Color{150, 180, 220, 255});
}

// ==================== ИНДИКАТОР БОССА ====================
void drawBossIndicator() {
    for (auto& z : G.zombies) {
        if (!z.isBoss) continue;
        Vector2 screen = GetWorldToScreen(z.pos, G.cam);
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (screen.x > 0 && screen.x < sw && screen.y > 0 && screen.y < sh) continue;
        float cx = sw / 2.0f, cy = sh / 2.0f;
        float angle = atan2f(screen.y - cy, screen.x - cx);
        float r = 280.0f;
        float arrowX = cx + cosf(angle) * r;
        float arrowY = cy + sinf(angle) * r;
        DrawCircle((int)arrowX, (int)arrowY, 20, RED);
        DrawCircleLines((int)arrowX, (int)arrowY, 20, WHITE);
        DrawTextEx(gFont, "!", {arrowX - 4, arrowY - 14}, 30, 1, WHITE);
    }
}

// ==================== HUD ====================
void drawHUD() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Player& p = G.p;
    int indicatorY = 20;
    if (p.godMode) { DrawRectangle(sw - 200, indicatorY, 180, 22, Color{80, 0, 80, 200}); DrawTextEx(gFont, "★ GOD", {(float)(sw - 190), (float)(indicatorY + 3)}, 16, 1, MAGENTA); indicatorY += 26; }
    if (p.freezeZombies) { DrawRectangle(sw - 200, indicatorY, 180, 22, Color{0, 60, 100, 200}); DrawTextEx(gFont, "❄ FREEZE", {(float)(sw - 190), (float)(indicatorY + 3)}, 16, 1, SKYBLUE); indicatorY += 26; }
    if (p.infiniteAmmo) { DrawRectangle(sw - 200, indicatorY, 180, 22, Color{60, 60, 0, 200}); DrawTextEx(gFont, "∞ AMMO", {(float)(sw - 190), (float)(indicatorY + 3)}, 16, 1, YELLOW); indicatorY += 26; }
    if (p.instantBuild) { DrawRectangle(sw - 200, indicatorY, 180, 22, Color{0, 60, 0, 200}); DrawTextEx(gFont, "⚡ BUILD", {(float)(sw - 190), (float)(indicatorY + 3)}, 16, 1, GREEN); indicatorY += 26; }
    if (p.damageMultiplier != 1.0f) { DrawRectangle(sw - 200, indicatorY, 180, 22, Color{80, 40, 0, 200}); DrawTextEx(gFont, TextFormat("DMG x%.1f", p.damageMultiplier), {(float)(sw - 190), (float)(indicatorY + 3)}, 16, 1, ORANGE); indicatorY += 26; }
    int cx = sw/2, cy = sh/2;
    DrawCircle(cx, cy, 8, Color{255,255,255,40});
    DrawCircleLines(cx, cy, 8, WHITE);
    DrawLine(cx-15, cy, cx-6, cy, WHITE); DrawLine(cx+6, cy, cx+15, cy, WHITE);
    DrawLine(cx, cy-15, cx, cy-6, WHITE); DrawLine(cx, cy+6, cx, cy+15, WHITE);
    DrawRectangle(20, 20, 300, 30, Color{0,0,0,180});
    DrawRectangle(22, 22, 296, 26, Color{58,20,20,255});
    float hpPct = p.hp / p.maxHp;
    DrawRectangle(22, 22, (int)(296*hpPct), 26, hpPct > 0.3f ? RED : Color{255,50,50,255});
    DrawRectangleLines(22, 22, 296, 26, BLACK);
    DrawTextEx(gFont, TextFormat("HP %d / %d", (int)p.hp, (int)p.maxHp), {30, 26}, 20, 1, WHITE);
    if (p.armor > 0) {
        DrawRectangle(20, 54, 300, 10, Color{0,0,0,180});
        DrawRectangle(22, 56, (int)(296 * p.armor/100.0f), 6, {138,153,168,255});
        DrawTextEx(gFont, TextFormat("Броня %d", (int)p.armor), {330, 52}, 14, 1, {138,153,168,255});
    }
    DrawRectangle(20, 68, 300, 10, Color{0,0,0,180});
    DrawRectangle(22, 70, (int)(296 * p.stamina/p.maxStamina), 6, GREEN);
    if (p.combo >= 3) {
        float ca = clampf(p.comboTimer / 2.5f, 0, 1);
        DrawTextEx(gFont, TextFormat("x%d КОМБО", p.combo), {20, 85}, 24, 1, (Color){255,155,61,(unsigned char)(255*ca)});
    }
    const char* labels[] = {"Д", "М", "Т", "Х"};
    int vals[] = {p.inv.wood, p.inv.metal, p.inv.cloth, p.inv.chem};
    Color cols[] = {{160,122,69,255},{138,153,168,255},{200,100,200,255},{200,200,50,255}};
    float rx = 20;
    for (int i = 0; i < 4; i++) {
        DrawRectangle((int)rx, 115, 70, 26, Color{0,0,0,180});
        DrawCircle((int)rx+13, 128, 9, cols[i]);
        DrawTextEx(gFont, labels[i], {rx+9, 121}, 14, 1, BLACK);
        DrawTextEx(gFont, TextFormat("%d", vals[i]), {rx+28, 118}, 16, 1, WHITE);
        rx += 76;
    }
    float wy = sh - 90;
    for (int i = 0; i < W_COUNT; i++) {
        if (!p.hasWeapon[i]) continue;
        float wcx = 20 + i*105;
        bool act = p.currentWeapon == i;
        DrawRectangle((int)wcx, (int)wy, 100, 70, act ? Color{20,50,25,220} : Color{0,0,0,180});
        DrawRectangleLines((int)wcx, (int)wy, 100, 70, act ? GREEN : GRAY);
        DrawTextEx(gFont, TextFormat("[%d]", i+1), {wcx+5, wy+5}, 12, 1, act ? GREEN : GRAY);
        DrawTextEx(gFont, WEAPONS[i].name, {wcx+5, wy+22}, 14, 1, act ? WHITE : LIGHTGRAY);
        DrawTextEx(gFont, TextFormat("%d", p.inv.ammo[WEAPONS[i].ammoType]), {wcx+5, wy+44}, 20, 1, p.inv.ammo[WEAPONS[i].ammoType] > 0 || p.infiniteAmmo ? (act ? GREEN : GRAY) : RED);
    }
    DrawTextEx(gFont, TextFormat("[E] Мед: %d", p.inv.medkits), {(float)sw - 180, (float)sh - 40}, 20, 1, p.inv.medkits > 0 ? GREEN : GRAY);
    DrawTextEx(gFont, TextFormat("Уровень %d / %d", G.level, LEVEL_COUNT), {(float)sw - 250, (float)(20 + indicatorY)}, 22, 1, WHITE);
    DrawTextEx(gFont, TextFormat("Очки: %d", G.score), {(float)sw - 250, (float)(48 + indicatorY)}, 18, 1, GOLD);
    DrawTextEx(gFont, TextFormat("Убито: %d", G.kills), {(float)sw - 250, (float)(70 + indicatorY)}, 18, 1, LIGHTGRAY);
    int remain = (int)G.zombies.size() + (int)G.spawnQueue.size();
    DrawTextEx(gFont, TextFormat("Осталось: %d", remain), {(float)sw - 250, (float)(92 + indicatorY)}, 18, 1, LIGHTGRAY);

    if (p.nearestBuilding >= 0 && !G.buildMode) {
        Structure& s = G.structures[p.nearestBuilding];
        int hintX = sw/2 - 300;
        int hintY = sh - 200;
        DrawRectangle(hintX, hintY, 600, 80, Color{0, 0, 20, 220});
        // ИСПРАВЛЕНО: SKYBLUE вместо CYAN
        DrawRectangleLines(hintX, hintY, 600, 80, SKYBLUE);
        DrawTextEx(gFont, TextFormat("%s — Уровень %d / 3  •  HP %d / %d",
                   BUILD_TYPES[s.type].name, s.level, (int)s.hp, (int)s.maxHp),
                   {(float)(hintX+15), (float)(hintY+10)}, 18, 1, WHITE);
        const char* upHint = s.level < 3 ? TextFormat("[U] Улучшить (Д%d М%d)", s.level==1?8:15, s.level==1?8:20) : "[U] Макс. уровень";
        DrawTextEx(gFont, upHint, {(float)(hintX+15), (float)(hintY+35)}, 16, 1, s.level < 3 ? GOLD : GRAY);
        const char* repHint = s.hp < s.maxHp ? TextFormat("[R] Починить (%d металла)", (int)ceilf((s.maxHp-s.hp)/40.0f)) : "[R] HP полное";
        DrawTextEx(gFont, repHint, {(float)(hintX+300), (float)(hintY+35)}, 16, 1, s.hp < s.maxHp ? GREEN : GRAY);
    }

    for (auto& z : G.zombies) {
        if (!z.isBoss) continue;
        int barW = 500, barH = 28;
        int bx = (sw - barW)/2, by = 20;
        DrawRectangle(bx-4, by-4, barW+8, barH+8, BLACK);
        DrawRectangle(bx, by, barW, barH, Color{40,10,20,255});
        float pct = z.hp / z.maxHp;
        Color barCol = z.phase == 0 ? Color{255,200,50,255} : z.phase == 1 ? Color{255,100,50,255} : Color{255,30,30,255};
        DrawRectangle(bx, by, (int)(barW*pct), barH, barCol);
        DrawRectangleLines(bx, by, barW, barH, barCol);
        const char* name = z.type == Z_BOSS_BRUTE ? "ГРОМИЛА" : z.type == Z_BOSS_QUEEN ? "КОРОЛЕВА" : "ПРИЗРАК";
        const char* phaseText = z.phase == 0 ? "Фаза 1" : z.phase == 1 ? "Фаза 2" : "ФАЗА 3 — ЯРОСТЬ";
        const char* fullText = TextFormat("%s — %s", name, phaseText);
        Vector2 ft = MeasureTextEx(gFont, fullText, 18, 1);
        DrawTextEx(gFont, fullText, {(float)sw/2 - ft.x/2, (float)(by + barH + 6)}, 18, 1, WHITE);
        break;
    }

    if (G.buildMode) {
        int cols = 6, cardW = 140, cardH = 65, gap = 5;
        int totalW = cols * cardW + (cols-1) * gap;
        int startX = (sw - totalW) / 2;
        int startY = sh - 160;
        for (int i = 0; i < BUILD_TYPE_COUNT; i++) {
            int row = i / cols, col = i % cols;
            const BuildTypeDef& bt = BUILD_TYPES[i];
            int bx = startX + col * (cardW + gap);
            int by = startY + row * (cardH + gap);
            bool act = (G.buildType == i);
            bool aff = p.instantBuild || (p.inv.wood >= bt.wood && p.inv.metal >= bt.metal && p.inv.cloth >= bt.cloth && p.inv.chem >= bt.chem);
            DrawRectangle(bx, by, cardW, cardH, act ? Color{20,50,25,220} : Color{0,0,0,180});
            DrawRectangleLines(bx, by, cardW, cardH, act ? GREEN : (aff ? GRAY : RED));
            const char* keyStr = i < 9 ? TextFormat("[%d]", i+1) : i == 9 ? "[0]" : i == 10 ? "[-]" : "[=]";
            DrawTextEx(gFont, keyStr, {(float)(bx+4), (float)(by+4)}, 11, 1, act ? GREEN : GRAY);
            DrawTextEx(gFont, bt.name, {(float)(bx+4), (float)(by+20)}, 13, 1, act ? WHITE : LIGHTGRAY);
            if (p.instantBuild) DrawTextEx(gFont, "БЕСПЛАТНО", {(float)(bx+4), (float)(by+42)}, 11, 1, GREEN);
            else {
                DrawTextEx(gFont, TextFormat("Д%d М%d", bt.wood, bt.metal), {(float)(bx+4), (float)(by+38)}, 10, 1, aff ? GRAY : RED);
                DrawTextEx(gFont, TextFormat("Т%d Х%d", bt.cloth, bt.chem), {(float)(bx+4), (float)(by+50)}, 10, 1, aff ? GRAY : RED);
            }
        }
        DrawTextEx(gFont, "1-9,0,-,= / колесо: выбор  •  ЛКМ: поставить  •  B: выход", {(float)sw/2 - 280, (float)(startY - 28)}, 16, 1, LIGHTGRAY);
    } else {
        DrawTextEx(gFont, "[TAB] Крафт  •  [B] Стройка  •  [U] Улучшить  •  [R] Починить  •  [ESC] Пауза",
                   {20, (float)sh - 30}, 16, 1, GRAY);
    }
    drawMinimap();
    drawBossIndicator();
    if (G.bannerTimer > 0) {
        float a = clampf(G.bannerTimer, 0, 1);
        Vector2 sz = MeasureTextEx(gFont, G.bannerText.c_str(), 50, 2);
        DrawTextEx(gFont, G.bannerText.c_str(), {(float)sw/2 - sz.x/2, 120}, 50, 2, {255,200,50,(unsigned char)(255*a)});
    }
    if (!G.message.empty() && G.messageTimer > 0) {
        Vector2 sz = MeasureTextEx(gFont, G.message.c_str(), 22, 1);
        DrawTextEx(gFont, G.message.c_str(), {(float)sw/2 - sz.x/2, (float)sh - 250}, 22, 1, GOLD);
    }
}

// ==================== МЕНЮ РАЗРАБОТЧИКА ====================
bool devButton(int x, int y, int w, int h, const char* label, bool active, Color baseColor) {
    Vector2 mouse = GetMousePosition();
    bool hover = (mouse.x >= x && mouse.x <= x+w && mouse.y >= y && mouse.y <= y+h);
    Color bg = hover ? Color{40, 40, 60, 255} : Color{20, 20, 30, 255};
    if (active) bg = hover ? Color{100, 40, 100, 255} : Color{70, 20, 70, 255};
    DrawRectangle(x, y, w, h, bg);
    DrawRectangleLines(x, y, w, h, active ? baseColor : GRAY);
    Vector2 ls = MeasureTextEx(gFont, label, 15, 1);
    DrawTextEx(gFont, label, {(float)x + (w - ls.x)/2, (float)y + (h - ls.y)/2}, 15, 1, active ? baseColor : WHITE);
    if (active) DrawTextEx(gFont, "✓", {(float)x + w - 22, (float)y + 5}, 18, 1, GREEN);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void drawDevMenu() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Color{0, 0, 15, 230});
    Vector2 ts = MeasureTextEx(gFont, "★ МЕНЮ РАЗРАБОТЧИКА ★", 44, 2);
    DrawTextEx(gFont, "★ МЕНЮ РАЗРАБОТЧИКА ★", {(float)sw/2 - ts.x/2, 20}, 44, 2, GOLD);
    Vector2 hs = MeasureTextEx(gFont, "[ESC] — закрыть", 16, 1);
    DrawTextEx(gFont, "[ESC] — закрыть", {(float)sw/2 - hs.x/2, 72}, 16, 1, LIGHTGRAY);
    const char* status = TextFormat("Д:%d М:%d Т:%d Х:%d | Мед:%d Броня:%d | DMG:x%.2f | HP:%d/%d",
        G.p.inv.wood, G.p.inv.metal, G.p.inv.cloth, G.p.inv.chem, G.p.inv.medkits, (int)G.p.armor, G.p.damageMultiplier, (int)G.p.hp, (int)G.p.maxHp);
    Vector2 ss = MeasureTextEx(gFont, status, 15, 1);
    DrawTextEx(gFont, status, {(float)sw/2 - ss.x/2, 100}, 15, 1, SKYBLUE);
    int colW = (sw - 200) / 3, col1X = 100, col2X = 100 + colW + 20, col3X = 100 + 2*(colW+20);
    int startY = 140, btnH = 32, gap = 8, y;

    y = startY;
    DrawTextEx(gFont, "РЕСУРСЫ", {(float)col1X, (float)y}, 18, 1, GREEN); y += 28;
    if (devButton(col1X, y, colW-10, btnH, "+100 Дерево", false, GREEN)) G.p.inv.wood += 100; y += btnH+gap;
    if (devButton(col1X, y, colW-10, btnH, "+100 Металл", false, GREEN)) G.p.inv.metal += 100; y += btnH+gap;
    if (devButton(col1X, y, colW-10, btnH, "+50 Ткань", false, GREEN)) G.p.inv.cloth += 50; y += btnH+gap;
    if (devButton(col1X, y, colW-10, btnH, "+50 Химия", false, GREEN)) G.p.inv.chem += 50; y += btnH+gap;
    if (devButton(col1X, y, colW-10, btnH, "+10 Аптечки", false, GREEN)) G.p.inv.medkits += 10; y += btnH+gap;
    y += 8;
    DrawTextEx(gFont, "ПАТРОНЫ", {(float)col1X, (float)y}, 18, 1, SKYBLUE); y += 28;
    if (devButton(col1X, y, colW-10, btnH, "+500 Пули", false, SKYBLUE)) G.p.inv.ammo[0] += 500; y += btnH+gap;
    if (devButton(col1X, y, colW-10, btnH, "+100 Картечь", false, SKYBLUE)) G.p.inv.ammo[1] += 100; y += btnH+gap;
    if (devButton(col1X, y, colW-10, btnH, "+50 Топливо", false, SKYBLUE)) G.p.inv.ammo[2] += 50; y += btnH+gap;
    y += 8;
    if (devButton(col1X, y, colW-10, btnH, "Открыть ВСЁ оружие", false, ORANGE)) for (int i = 0; i < W_COUNT; i++) G.p.hasWeapon[i] = true;

    y = startY;
    DrawTextEx(gFont, "УРОН", {(float)col2X, (float)y}, 18, 1, ORANGE); y += 28;
    int halfW = (colW - 20) / 2;
    if (devButton(col2X, y, halfW, btnH, "x0.1", false, ORANGE)) G.p.damageMultiplier = 0.1f;
    if (devButton(col2X + halfW + 10, y, halfW, btnH, "x1", false, ORANGE)) G.p.damageMultiplier = 1.0f; y += btnH+gap;
    if (devButton(col2X, y, halfW, btnH, "x10", false, ORANGE)) G.p.damageMultiplier = 10.0f;
    if (devButton(col2X + halfW + 10, y, halfW, btnH, "x100", false, ORANGE)) G.p.damageMultiplier = 100.0f; y += btnH+gap;
    if (devButton(col2X, y, halfW, btnH, "x1000", false, ORANGE)) G.p.damageMultiplier = 1000.0f;
    if (devButton(col2X + halfW + 10, y, halfW, btnH, "x99999", false, ORANGE)) G.p.damageMultiplier = 99999.0f; y += btnH+gap;
    y += 8;
    if (devButton(col2X, y, colW-10, btnH, "Полное HP", false, RED)) G.p.hp = G.p.maxHp; y += btnH+gap;
    if (devButton(col2X, y, colW-10, btnH, "+1000 макс HP", false, RED)) { G.p.maxHp += 1000; G.p.hp = G.p.maxHp; } y += btnH+gap;
    if (devButton(col2X, y, colW-10, btnH, "+100 Броня", false, {138,153,168,255})) G.p.armor = fminf(100, G.p.armor + 100); y += btnH+gap;

    y = startY;
    DrawTextEx(gFont, "ЧИТЫ", {(float)col3X, (float)y}, 18, 1, MAGENTA); y += 28;
    if (devButton(col3X, y, colW-10, btnH, "Бог", G.p.godMode, MAGENTA)) G.p.godMode = !G.p.godMode; y += btnH+gap;
    if (devButton(col3X, y, colW-10, btnH, "∞ Патроны", G.p.infiniteAmmo, MAGENTA)) G.p.infiniteAmmo = !G.p.infiniteAmmo; y += btnH+gap;
    if (devButton(col3X, y, colW-10, btnH, "Заморозка", G.p.freezeZombies, MAGENTA)) G.p.freezeZombies = !G.p.freezeZombies; y += btnH+gap;
    if (devButton(col3X, y, colW-10, btnH, "Бесплатная стройка", G.p.instantBuild, MAGENTA)) G.p.instantBuild = !G.p.instantBuild; y += btnH+gap;
    y += 8;
    DrawTextEx(gFont, "УРОВЕНЬ", {(float)col3X, (float)y}, 18, 1, GOLD); y += 28;
    if (devButton(col3X, y, colW-10, btnH, "Пропустить", false, GOLD)) {
        for (auto& z : G.zombies) z.hp = 0;
        G.spawnQueue.clear(); G.zombies.clear();
        G.levelStarted = false; G.state = ST_LEVEL_COMPLETE;
    } y += btnH+gap;
    if (devButton(col3X, y, colW-10, btnH, "Убить всех", false, GOLD)) for (int i = (int)G.zombies.size()-1; i >= 0; i--) killZombie(i); y += btnH+gap;
    if (devButton(col3X, y, colW-10, btnH, "+10000 очков", false, GOLD)) G.score += 10000; y += btnH+gap;
    if (devButton(col3X, y, colW-10, btnH, "Сброс читов", false, RED)) {
        G.p.godMode = false; G.p.infiniteAmmo = false;
        G.p.freezeZombies = false; G.p.instantBuild = false;
        G.p.damageMultiplier = 1.0f;
    } y += btnH+gap;
}

// ==================== КРАФТ ====================
void drawCraftScreen() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Player& p = G.p;
    DrawRectangle(0, 0, sw, sh, Color{0,0,0,220});
    Vector2 ts = MeasureTextEx(gFont, "КРАФТ", 52, 2);
    DrawTextEx(gFont, "КРАФТ", {(float)sw/2 - ts.x/2, 40}, 52, 2, GREEN);
    Vector2 hs = MeasureTextEx(gFont, "Выбери рецепт (клик или 1-9)  •  TAB — закрыть", 18, 1);
    DrawTextEx(gFont, "Выбери рецепт (клик или 1-9)  •  TAB — закрыть", {(float)sw/2 - hs.x/2, 100}, 18, 1, LIGHTGRAY);
    int gridCols = 4, cardW = 200, cardH = 150, gap = 12;
    int totalW = gridCols*cardW + (gridCols-1)*gap;
    int sx = (sw - totalW)/2, sy = 150;
    Vector2 mouse = GetMousePosition();
    G.craftHover = -1;
    for (int i = 0; i < RECIPE_COUNT; i++) {
        int col = i % gridCols, row = i / gridCols;
        float cx = (float)(sx + col*(cardW+gap)), cy = (float)(sy + row*(cardH+gap));
        bool hov = (mouse.x >= cx && mouse.x <= cx+cardW && mouse.y >= cy && mouse.y <= cy+cardH);
        bool affordable = canCraft(RECIPES[i]);
        if (hov) G.craftHover = i;
        DrawRectangle((int)cx, (int)cy, cardW, cardH, hov ? Color{20,30,25,255} : Color{10,15,10,255});
        DrawRectangleLines((int)cx, (int)cy, cardW, cardH, affordable ? (hov ? GREEN : GRAY) : RED);
        Color titleCol = (RECIPES[i].type == 0) ? ORANGE : (RECIPES[i].type == 1) ? SKYBLUE : (RECIPES[i].type == 2) ? GREEN : GOLD;
        DrawTextEx(gFont, RECIPES[i].name, {cx+8, cy+8}, 18, 1, titleCol);
        DrawTextEx(gFont, RECIPES[i].desc, {cx+8, cy+32}, 13, 1, LIGHTGRAY);
        float yy = cy + 55;
        if (RECIPES[i].w > 0) { DrawTextEx(gFont, TextFormat("Д: %d", RECIPES[i].w), {cx+8, yy}, 13, 1, p.inv.wood >= RECIPES[i].w ? GRAY : RED); yy += 15; }
        if (RECIPES[i].m > 0) { DrawTextEx(gFont, TextFormat("М: %d", RECIPES[i].m), {cx+8, yy}, 13, 1, p.inv.metal >= RECIPES[i].m ? GRAY : RED); yy += 15; }
        if (RECIPES[i].c > 0) { DrawTextEx(gFont, TextFormat("Т: %d", RECIPES[i].c), {cx+8, yy}, 13, 1, p.inv.cloth >= RECIPES[i].c ? GRAY : RED); yy += 15; }
        if (RECIPES[i].ch > 0) { DrawTextEx(gFont, TextFormat("Х: %d", RECIPES[i].ch), {cx+8, yy}, 13, 1, p.inv.chem >= RECIPES[i].ch ? GRAY : RED); yy += 15; }
        if (RECIPES[i].type == 0 && p.hasWeapon[RECIPES[i].result]) DrawTextEx(gFont, "УЖЕ ЕСТЬ", {cx+cardW-85, cy+cardH-20}, 13, 1, GRAY);
        else DrawTextEx(gFont, "КРАФТ", {cx+cardW-60, cy+cardH-20}, 14, 1, affordable ? GREEN : GRAY);
        if (i < 9) DrawTextEx(gFont, TextFormat("[%d]", i+1), {cx+cardW-30, cy+8}, 13, 1, GRAY);
        if (hov && affordable && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) doCraft(i);
    }
    for (int i = 0; i < 9 && i < RECIPE_COUNT; i++) if (IsKeyPressed(KEY_ONE + i) && canCraft(RECIPES[i])) doCraft(i);
    float ry = (float)(sh - 80);
    DrawTextEx(gFont, "Ресурсы:", {20, ry - 25}, 16, 1, GRAY);
    const char* labels[] = {"Дерево", "Металл", "Ткань", "Химия", "Медкит", "Броня"};
    int vals[] = {p.inv.wood, p.inv.metal, p.inv.cloth, p.inv.chem, p.inv.medkits, (int)p.armor};
    Color invColors[] = {{160,122,69,255},{138,153,168,255},{200,100,200,255},{200,200,50,255},{90,223,122,255},{180,180,220,255}};
    float rx = 20.0f;
    for (int i = 0; i < 6; i++) {
        DrawRectangle((int)rx, (int)ry, 130, 40, Color{0,0,0,180});
        DrawCircle((int)rx+18, (int)ry+20, 11, invColors[i]);
        DrawTextEx(gFont, labels[i], {rx+34, ry+8}, 13, 1, LIGHTGRAY);
        DrawTextEx(gFont, TextFormat("%d", vals[i]), {rx+34, ry+22}, 16, 1, WHITE);
        rx += 140;
    }
}

// ==================== ПАУЗА / ЭКРАНЫ ====================
void drawPauseScreen() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Color{0,0,0,190});
    Vector2 ts = MeasureTextEx(gFont, "ПАУЗА", 96, 3);
    DrawTextEx(gFont, "ПАУЗА", {(float)sw/2 - ts.x/2, (float)sh/2 - 150}, 96, 3, GOLD);
    const char* info = TextFormat("Уровень %d    Очки %d    Убито %d", G.level, G.score, G.kills);
    Vector2 is = MeasureTextEx(gFont, info, 24, 1);
    DrawTextEx(gFont, info, {(float)sw/2 - is.x/2, (float)sh/2 - 20}, 24, 1, WHITE);
    const char* controls[] = {"[ESC] — продолжить", "[M] — в главное меню", "[Q] — выйти из игры"};
    for (int i = 0; i < 3; i++) {
        Vector2 cs = MeasureTextEx(gFont, controls[i], 22, 1);
        Color cc = (i == 0) ? GREEN : (i == 1) ? LIGHTGRAY : RED;
        DrawTextEx(gFont, controls[i], {(float)sw/2 - cs.x/2, (float)sh/2 + 50 + i*40}, 22, 1, cc);
    }
}

void drawMenu() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ClearBackground(Color{10,12,9,255});
    for (int x = 0; x < sw; x += 80) DrawLine(x, 0, x, sh, Color{20,30,20,255});
    for (int y = 0; y < sh; y += 80) DrawLine(0, y, sw, y, Color{20,30,20,255});
    Vector2 ts = MeasureTextEx(gFont, "ЗОМБИ 3D", 120, 3);
    DrawTextEx(gFont, "ЗОМБИ 3D", {(float)sw/2 - ts.x/2, (float)sh/2 - 220}, 120, 3, RED);
    Vector2 ss = MeasureTextEx(gFont, "В Ы Ж И В А Н И Е", 36, 2);
    DrawTextEx(gFont, "В Ы Ж И В А Н И Е", {(float)sw/2 - ss.x/2, (float)sh/2 - 80}, 36, 2, LIGHTGRAY);
    float pulse = (sinf(GetTime()*4) + 1) * 0.5f;
    Vector2 ps = MeasureTextEx(gFont, "НАЖМИ ENTER ИЛИ КЛИКНИ", 28, 1);
    DrawTextEx(gFont, "НАЖМИ ENTER ИЛИ КЛИКНИ", {(float)sw/2 - ps.x/2, (float)sh/2 + 30}, 28, 1, Color{90,223,122,(unsigned char)(150+pulse*105)});
    const char* controls[] = {
        "WASD — движение  •  Мышь — прицел",
        "ЛКМ — огонь  •  1-5 — оружие  •  TAB — крафт (12)",
        "B — стройка (12)  •  U — улучшить  •  R — починить",
        "SPACE — рывок  •  SHIFT — бег  •  ESC — пауза  •  F11 — экран",
    };
    for (int i = 0; i < 4; i++) {
        Vector2 sz = MeasureTextEx(gFont, controls[i], 18, 1);
        DrawTextEx(gFont, controls[i], {(float)sw/2 - sz.x/2, (float)sh/2 + 100 + i*30}, 18, 1, GRAY);
    }
}

void drawGameOver() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Color{0,0,0,220});
    Vector2 ts = MeasureTextEx(gFont, "ВЫ ПОГИБЛИ", 72, 3);
    DrawTextEx(gFont, "ВЫ ПОГИБЛИ", {(float)sw/2 - ts.x/2, (float)sh/2 - 80}, 72, 3, RED);
    const char* info = TextFormat("Уровень: %d    Очки: %d    Убито: %d    Лучшее комбо: x%d", G.level, G.score, G.kills, G.p.bestCombo);
    Vector2 is = MeasureTextEx(gFont, info, 26, 1);
    DrawTextEx(gFont, info, {(float)sw/2 - is.x/2, (float)sh/2}, 26, 1, WHITE);
    Vector2 hs = MeasureTextEx(gFont, "[R] Ещё раз  •  [ESC] в меню", 28, 1);
    DrawTextEx(gFont, "[R] Ещё раз  •  [ESC] в меню", {(float)sw/2 - hs.x/2, (float)sh/2 + 80}, 28, 1, GREEN);
}

void drawLevelComplete() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Color{0,0,0,220});
    const char* t = TextFormat("УРОВЕНЬ %d ПРОЙДЕН", G.level);
    Vector2 ts = MeasureTextEx(gFont, t, 64, 2);
    DrawTextEx(gFont, t, {(float)sw/2 - ts.x/2, (float)sh/2 - 150}, 64, 2, GREEN);
    const char* s1 = TextFormat("Очки: %d", G.score);
    const char* s2 = TextFormat("Убито: %d", G.kills);
    const char* s3 = TextFormat("Комбо: x%d", G.p.bestCombo);
    Vector2 vs1 = MeasureTextEx(gFont, s1, 26, 1);
    Vector2 vs2 = MeasureTextEx(gFont, s2, 26, 1);
    Vector2 vs3 = MeasureTextEx(gFont, s3, 26, 1);
    DrawTextEx(gFont, s1, {(float)sw/2 - vs1.x/2, (float)sh/2 - 40}, 26, 1, WHITE);
    DrawTextEx(gFont, s2, {(float)sw/2 - vs2.x/2, (float)sh/2}, 26, 1, WHITE);
    DrawTextEx(gFont, s3, {(float)sw/2 - vs3.x/2, (float)sh/2 + 40}, 26, 1, WHITE);
    float pulse = (sinf(GetTime()*4) + 1) * 0.5f;
    Vector2 cs = MeasureTextEx(gFont, "ENTER — следующий уровень", 28, 1);
    DrawTextEx(gFont, "ENTER — следующий уровень", {(float)sw/2 - cs.x/2, (float)sh/2 + 130}, 28, 1, Color{90,223,122,(unsigned char)(150+pulse*105)});
}

void drawVictory() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Color{0,0,0,230});
    Vector2 ts = MeasureTextEx(gFont, "ПОБЕДА!", 96, 3);
    DrawTextEx(gFont, "ПОБЕДА!", {(float)sw/2 - ts.x/2, (float)sh/2 - 150}, 96, 3, GOLD);
    Vector2 ss = MeasureTextEx(gFont, "Ты прошёл все 15 уровней!", 32, 1);
    DrawTextEx(gFont, "Ты прошёл все 15 уровней!", {(float)sw/2 - ss.x/2, (float)sh/2 - 30}, 32, 1, WHITE);
    const char* stats = TextFormat("Финальный счёт: %d  •  Убито: %d", G.score, G.kills);
    Vector2 st = MeasureTextEx(gFont, stats, 24, 1);
    DrawTextEx(gFont, stats, {(float)sw/2 - st.x/2, (float)sh/2 + 30}, 24, 1, LIGHTGRAY);
    Vector2 cs = MeasureTextEx(gFont, "ENTER — бесконечный режим", 28, 1);
    DrawTextEx(gFont, "ENTER — бесконечный режим", {(float)sw/2 - cs.x/2, (float)sh/2 + 100}, 28, 1, GREEN);
}

// ==================== MAIN ====================
int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Зомби 3D: Выживание");
    SetTargetFPS(60);
    EnableCursor();
    g_rng.seed(std::random_device{}());
    const char* fontPaths[] = {
        "C:/Windows/Fonts/arial.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf", "arial.ttf"
    };
    int codepoints[512]; int cpCount = 0;
    for (int c = 32; c < 127; c++) codepoints[cpCount++] = c;
    for (int c = 0x400; c <= 0x4FF; c++) codepoints[cpCount++] = c;
    gFont = GetFontDefault();
    for (auto path : fontPaths) {
        if (FileExists(path)) {
            Font f = LoadFontEx(path, 32, codepoints, cpCount);
            if (f.texture.id != 0) { gFont = f; break; }
        }
    }
    RenderTexture2D target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    const char* fragSrc =
        "#version 330\n"
        "in vec2 fragTexCoord;out vec4 finalColor;\n"
        "uniform sampler2D texture0;uniform vec2 screenSize;\n"
        "uniform float time;uniform float lowHP;\n"
        "float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
        "vec3 tonemap(vec3 x){return x/(x+vec3(1.0));}\n"
        "void main(){\n"
        "vec2 uv=fragTexCoord;float ca=0.002;\n"
        "float r=texture(texture0,uv+vec2(ca,0)).r;\n"
        "float g=texture(texture0,uv).g;\n"
        "float b=texture(texture0,uv-vec2(ca,0)).b;\n"
        "vec3 col=vec3(r,g,b);\n"
        "vec3 bloom=vec3(0.0);float wsum=0.0;\n"
        "for(int x=-4;x<=4;x++)for(int y=-4;y<=4;y++){\n"
        "float w=exp(-float(x*x+y*y)/8.0);\n"
        "vec2 off=vec2(float(x),float(y))*8.0/screenSize;\n"
        "vec3 s=texture(texture0,uv+off).rgb;\n"
        "float lum=dot(s,vec3(0.299,0.587,0.114));\n"
        "if(lum>0.55)bloom+=s*(lum-0.55)*w;wsum+=w;}\n"
        "bloom/=max(wsum,1.0);col+=bloom*1.3;\n"
        "float gray=dot(col,vec3(0.299,0.587,0.114));\n"
        "col=mix(vec3(gray),col,1.35);\n"
        "col=(col-0.5)*1.15+0.5;\n"
        "vec2 vu=uv*2.0-1.0;float vig=1.0-dot(vu,vu)*0.5;\n"
        "vig=clamp(vig,0.0,1.0);col*=vig;\n"
        "float grain=(hash(uv*screenSize+time)-0.5)*0.04;\n"
        "col+=grain;\n"
        "col*=vec3(1.05,1.0,0.95);\n"
        "vec3 rv=col*vec3(1.2,0.4,0.4);\n"
        "col=mix(col,rv,lowHP*(1.0-vig));\n"
        "col=tonemap(col*1.2);\n"
        "finalColor=vec4(col,1.0);}\n";
    Shader postShader = LoadShaderFromMemory(nullptr, fragSrc);
    bool useShader = (postShader.id != 0);
    int locScreen = GetShaderLocation(postShader, "screenSize");
    int locTime   = GetShaderLocation(postShader, "time");
    int locLowHP  = GetShaderLocation(postShader, "lowHP");
    G.reset();
    G.state = ST_MENU;
    EnableCursor();
    static GState prevState = ST_MENU;
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;
        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
            UnloadRenderTexture(target);
            target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        }
        bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (ctrlDown && shiftDown && IsKeyPressed(KEY_D)) {
            if (G.state == ST_PLAYING) { G.state = ST_DEV; EnableCursor(); }
            else if (G.state == ST_DEV) { G.state = ST_PLAYING; DisableCursor(); }
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (G.state == ST_MENU) break;
            else if (G.state == ST_PLAYING) { G.state = ST_PAUSE; EnableCursor(); }
            else if (G.state == ST_PAUSE) { G.state = ST_PLAYING; DisableCursor(); }
            else if (G.state == ST_CRAFT) { G.state = ST_PLAYING; DisableCursor(); }
            else if (G.state == ST_DEV) { G.state = ST_PLAYING; DisableCursor(); }
            else if (G.state == ST_GAMEOVER || G.state == ST_LEVEL_COMPLETE) {
                G.reset(); G.state = ST_MENU; EnableCursor();
            }
        }
        if (G.state == ST_PAUSE) {
            if (IsKeyPressed(KEY_M)) { G.reset(); G.state = ST_MENU; EnableCursor(); }
            if (IsKeyPressed(KEY_Q)) break;
        }
        if (G.state != prevState) {
            if (G.state == ST_PLAYING) DisableCursor();
            else EnableCursor();
            prevState = G.state;
        }
        if (G.state == ST_MENU) {
            if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { G.reset(); DisableCursor(); }
        } else if (G.state == ST_GAMEOVER) {
            if (IsKeyPressed(KEY_R)) { G.reset(); DisableCursor(); }
        } else if (G.state == ST_LEVEL_COMPLETE) {
            if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                G.level++;
                if (G.level > LEVEL_COUNT) G.level = LEVEL_COUNT + 1;
                G.state = ST_PLAYING;
                startLevel();
                DisableCursor();
            }
        } else if (G.state == ST_VICTORY) {
            if (IsKeyPressed(KEY_ENTER)) { G.level = 16; G.state = ST_PLAYING; startLevel(); DisableCursor(); }
        } else if (G.state == ST_PLAYING || G.state == ST_CRAFT) {
            updateGame(dt);
        }
        if (G.state == ST_MENU) {
            BeginDrawing();
            ClearBackground(Color{10,12,9,255});
            drawMenu();
            EndDrawing();
        } else if (G.state == ST_VICTORY) {
            BeginDrawing();
            ClearBackground(Color{0,0,0,255});
            drawVictory();
            EndDrawing();
        } else {
            BeginTextureMode(target);
            ClearBackground(Color{25, 35, 55, 255});
            int tw = target.texture.width, th = target.texture.height;
            DrawRectangleGradientV(0, 0, tw, th/2, Color{50, 90, 160, 255}, Color{160, 100, 120, 255});
            DrawRectangleGradientV(0, th/2, tw, th/2, Color{160, 100, 120, 255}, Color{40, 30, 50, 255});
            float sunX = tw * 0.75f, sunY = th * 0.25f;
            DrawCircle((int)sunX, (int)sunY, 60, Color{255, 220, 150, 80});
            DrawCircle((int)sunX, (int)sunY, 40, Color{255, 240, 180, 180});
            DrawCircle((int)sunX, (int)sunY, 30, Color{255, 250, 220, 255});
            for (int i = 0; i < 80; i++) {
                float sx = fmodf(i * 137.3f + 13.7f, (float)tw);
                float sy = fmodf(i * 71.1f + 7.3f, th * 0.4f);
                float twk = 0.5f + 0.5f * sinf(G.time * 2 + i);
                DrawCircle((int)sx, (int)sy, 1, Color{255, 255, 255, (unsigned char)(80 + twk*120)});
            }
            BeginMode3D(G.cam);
            drawWorld3D();
            EndMode3D();
            EndTextureMode();
            BeginDrawing();
            ClearBackground(BLACK);
            if (useShader) {
                BeginShaderMode(postShader);
                float ss[2] = {(float)GetScreenWidth(), (float)GetScreenHeight()};
                SetShaderValue(postShader, locScreen, ss, SHADER_UNIFORM_VEC2);
                float tm = (float)GetTime();
                SetShaderValue(postShader, locTime, &tm, SHADER_UNIFORM_FLOAT);
                float lowHP = (G.p.hp < G.p.maxHp * 0.3f) ? (1.0f - G.p.hp / (G.p.maxHp * 0.3f)) : 0.0f;
                SetShaderValue(postShader, locLowHP, &lowHP, SHADER_UNIFORM_FLOAT);
                DrawTextureRec(target.texture, (Rectangle){0, 0, (float)target.texture.width, (float)-target.texture.height}, (Vector2){0, 0}, WHITE);
                EndShaderMode();
            } else {
                DrawTextureRec(target.texture, (Rectangle){0, 0, (float)target.texture.width, (float)-target.texture.height}, (Vector2){0, 0}, WHITE);
            }
            drawRain();
            drawHUD();
            if (G.state == ST_CRAFT) drawCraftScreen();
            if (G.state == ST_PAUSE) drawPauseScreen();
            if (G.state == ST_DEV) drawDevMenu();
            if (G.state == ST_GAMEOVER) drawGameOver();
            if (G.state == ST_LEVEL_COMPLETE) drawLevelComplete();
            EndDrawing();
        }
    }
    UnloadShader(postShader);
    UnloadRenderTexture(target);
    UnloadFont(gFont);
    CloseWindow();
    return 0;
}