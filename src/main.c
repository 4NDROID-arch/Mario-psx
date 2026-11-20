#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <psxgpu.h>
#include <psxpad.h>
#include <psxapi.h>
#include <psxetc.h>

#define W 320
#define H 240
#define OT_LEN 8
#define PACKET_LEN 24576
#define TILE_SIZE 16
#define MAP_W 128
#define MAP_H 15
#define SOLID 1
#define BRICK 2
#define QUESTION 3
#define PIPE 4
#define COIN 5
#define ENEMY 6
#define GOAL 7

typedef struct {
    DISPENV disp;
    DRAWENV draw;
    uint32_t ot[OT_LEN];
    uint8_t packet[PACKET_LEN];
} Frame;

static Frame frames[2];
static int active = 0;
static uint8_t *next_packet;
static char pad_buffer[2][34];
static uint8_t map_data[3][MAP_H][MAP_W];
static int stage_index = 0;
static int camera_x = 0;
static int player_x = 32, player_y = 176;
static int player_vx = 0, player_vy = 0;
static int grounded = 0;
static int coins = 0;
static int enemy_x = 620, enemy_dir = 1;
static int dead = 0;
static int won = 0;

static void *alloc_prim(int z, int size) {
    void *p = next_packet;
    addPrim(&frames[active].ot[z], p);
    next_packet += size;
    return p;
}

static void rect(int x, int y, int w, int h, int r, int g, int b, int z) {
    TILE *t = (TILE *)alloc_prim(z, sizeof(TILE));
    setTile(t);
    setXY0(t, x, y);
    setWH(t, w, h);
    setRGB0(t, r, g, b);
}

static void text(int x, int y, const char *s) {
    next_packet = (uint8_t *)FntSort(&frames[active].ot[0], next_packet, x, y, s);
}

static void setup_frame(Frame *f, int y) {
    SetDefDispEnv(&f->disp, 0, y, W, H);
    SetDefDrawEnv(&f->draw, 0, y ? 0 : H, W, H);
    setRGB0(&f->draw, 92, 148, 252);
    f->draw.isbg = 1;
    f->draw.dtd = 1;
    ClearOTagR(f->ot, OT_LEN);
}

static void stage_build(int s) {
    int x, y;
    memset(map_data[s], 0, sizeof(map_data[s]));
    for (x = 0; x < MAP_W; x++) {
        map_data[s][13][x] = SOLID;
        map_data[s][14][x] = BRICK;
    }

    if (s == 0) {
        for (x=13;x<18;x++) map_data[s][9][x] = BRICK;
        map_data[s][9][22] = QUESTION;
        map_data[s][9][23] = BRICK;
        map_data[s][9][24] = BRICK;
        map_data[s][9][25] = QUESTION;
        for (x=36;x<40;x++) map_data[s][10][x] = BRICK;
        for (x=49;x<53;x++) map_data[s][8][x] = BRICK;
        for (x=70;x<73;x++) map_data[s][11][x] = PIPE;
        for (y=11;y<13;y++) map_data[s][y][70] = map_data[s][y][71] = map_data[s][y][72] = PIPE;
        for (x=88;x<92;x++) map_data[s][9][x] = QUESTION;
    } else if (s == 1) {
        for (x=10;x<15;x++) map_data[s][10][x] = BRICK;
        for (x=24;x<28;x++) map_data[s][8][x] = QUESTION;
        for (x=39;x<44;x++) map_data[s][11][x] = BRICK;
        for (x=57;x<61;x++) map_data[s][9][x] = BRICK;
        for (y=10;y<13;y++) for (x=78;x<82;x++) map_data[s][y][x] = PIPE;
        for (x=96;x<101;x++) map_data[s][8][x] = BRICK;
    } else {
        for (x=12;x<18;x++) map_data[s][8][x] = BRICK;
        for (x=29;x<34;x++) map_data[s][10][x] = QUESTION;
        for (x=46;x<50;x++) map_data[s][7][x] = BRICK;
        for (y=9;y<13;y++) for (x=64;x<68;x++) map_data[s][y][x] = PIPE;
        for (x=84;x<90;x++) map_data[s][9][x] = BRICK;
        for (x=106;x<111;x++) map_data[s][7][x] = QUESTION;
    }
    map_data[s][12][MAP_W-5] = GOAL;
}

static void reset_stage(void) {
    player_x = 32; player_y = 176;
    player_vx = player_vy = 0;
    grounded = 0; camera_x = 0;
    enemy_x = 560 + stage_index * 35; enemy_dir = 1;
    dead = 0; won = 0;
}

static int solid_at(int px, int py) {
    int tx = px / TILE_SIZE;
    int ty = py / TILE_SIZE;
    int v;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;
    v = map_data[stage_index][ty][tx];
    return v == SOLID || v == BRICK || v == PIPE;
}

static void update_game(void) {
    PADTYPE *pad = (PADTYPE *)pad_buffer[0];
    int left = 0, right = 0, jump = 0;
    if (pad->stat == 0) {
        left = !(pad->btn & PAD_LEFT);
        right = !(pad->btn & PAD_RIGHT);
        jump = !(pad->btn & PAD_CROSS);
    }
    if (dead || won) {
        if (pad->stat == 0 && !(pad->btn & PAD_START)) {
            if (won) stage_index = (stage_index + 1) % 3;
            reset_stage();
        }
        return;
    }
    player_vx = left ? -2 : (right ? 2 : 0);
    if (jump && grounded) {
        player_vy = -7;
        grounded = 0;
    }
    player_vy += 1;
    if (player_vy > 7) player_vy = 7;

    player_x += player_vx;
    if (player_x < 0) player_x = 0;
    if (player_vx > 0 && solid_at(player_x+15, player_y+8)) player_x = ((player_x+15)/16)*16-16;
    if (player_vx < 0 && solid_at(player_x, player_y+8)) player_x = ((player_x/16)+1)*16;

    player_y += player_vy;
    if (player_vy >= 0 && (solid_at(player_x+2, player_y+16) || solid_at(player_x+14, player_y+16))) {
        player_y = ((player_y+16)/16)*16-16;
        player_vy = 0; grounded = 1;
    } else {
        grounded = 0;
    }

    enemy_x += enemy_dir;
    if (enemy_x < 470 || enemy_x > 720) enemy_dir = -enemy_dir;

    if (player_x > camera_x + 180) camera_x = player_x - 180;
    if (camera_x < 0) camera_x = 0;
    if (camera_x > MAP_W*TILE_SIZE-W) camera_x = MAP_W*TILE_SIZE-W;

    if (player_y > H) dead = 1;
    if (player_x > (MAP_W-7)*TILE_SIZE) won = 1;
}

static void draw_scene(void) {
    int x, y, t, sx;
    rect(0,0,W,H,92,148,252,7);

    /* distant hills and clouds */
    rect(22,132,70,24,40,184,72,6);
    rect(40,116,38,20,40,184,72,6);
    rect(190,142,88,20,40,184,72,6);
    rect(205,126,48,18,40,184,72,6);
    rect(50,42,38,7,255,255,255,6);
    rect(62,34,20,12,255,255,255,6);
    rect(222,58,42,7,255,255,255,6);

    for (x=0;x<MAP_W;x++) {
        sx = x*TILE_SIZE-camera_x;
        if (sx < -16 || sx >= W) continue;
        for (y=0;y<MAP_H;y++) {
            t = map_data[stage_index][y][x];
            if (!t) continue;
            if (t == SOLID) {
                rect(sx,y*16,16,16,176,72,32,4);
                rect(sx,y*16,16,3,248,168,64,3);
            } else if (t == BRICK) {
                rect(sx,y*16,16,16,188,92,40,4);
                rect(sx,y*16+7,16,2,112,48,24,3);
                rect(sx+7,y*16,2,7,112,48,24,3);
            } else if (t == QUESTION) {
                rect(sx,y*16,16,16,248,184,48,4);
                rect(sx+4,y*16+3,8,3,255,240,160,3);
                rect(sx+7,y*16+7,3,5,255,240,160,3);
            } else if (t == PIPE) {
                rect(sx,y*16,16,16,24,168,48,4);
                rect(sx+3,y*16+3,4,13,112,224,96,3);
            } else if (t == GOAL) {
                rect(sx+7,y*16-64,2,80,232,232,232,3);
                rect(sx+9,y*16-62,18,12,248,64,64,3);
            }
        }
    }

    /* Original creature enemy */
    rect(enemy_x-camera_x,192,16,12,120,56,160,2);
    rect(enemy_x-camera_x+3,195,3,3,255,255,255,1);
    rect(enemy_x-camera_x+10,195,3,3,255,255,255,1);

    /* Original hero: red cap, teal shirt, brown boots */
    rect(player_x-camera_x+3,player_y,10,5,224,48,40,1);
    rect(player_x-camera_x+2,player_y+5,12,4,248,112,72,1);
    rect(player_x-camera_x+3,player_y+9,10,6,32,144,152,1);
    rect(player_x-camera_x+1,player_y+15,6,4,112,64,32,1);
    rect(player_x-camera_x+9,player_y+15,6,4,112,64,32,1);
    rect(player_x-camera_x+10,player_y+6,2,2,24,24,24,0);

    text(8,8,"VIZO PLATFORMER");
    text(8,20,"D-PAD MOVE   X JUMP");
    if (dead) text(105,110,"FELL DOWN - START RETRY");
    if (won) text(100,110,"STAGE CLEAR - START NEXT");
}

int main(void) {
    int i;
    ResetGraph(0);
    setup_frame(&frames[0],0);
    setup_frame(&frames[1],H);
    FntLoad(960,0);
    FntOpen(8,8,304,40,0,64);
    EnterCriticalSection();
    InitPAD(&pad_buffer[0][0],34,&pad_buffer[1][0],34);
    StartPAD();
    ChangeClearPAD(0);
    ExitCriticalSection();

    stage_build(0);
    stage_build(1);
    stage_build(2);
    reset_stage();

    for (;;) {
        update_game();
        next_packet = frames[active].packet;
        ClearOTagR(frames[active].ot,OT_LEN);
        draw_scene();
        FntFlush(-1);
        DrawSync(0);
        VSync(0);
        PutDispEnv(&frames[active].disp);
        PutDrawEnv(&frames[active].draw);
        SetDispMask(1);
        DrawOTag(&frames[active].ot[OT_LEN-1]);
        active ^= 1;
    }
    return 0;
}
