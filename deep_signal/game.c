/* DEEP SIGNAL — original C64 exploration shooter, Codex, 2026-09-05.
 * Keyboard and joystick inputs feed the same rules. No engine/game code
 * copied from IRON VEIN. Art and map are authored in assetgen.py.
 */
#include <stdint.h>
#include <string.h>
#include <peekpoke.h>

#define RAM ((unsigned char*)0x8000)
#define SCREEN ((unsigned char*)0x4000)
#define COLOR ((unsigned char*)0xd800)
#define INPUT_PULSE 0x033c
#define INPUT_HOLD 0x033e
#define DIAGNOSTIC 0x033f
#define UP 1
#define DOWN 2
#define LEFT 4
#define RIGHT 8
#define FIRE 16
#define DEMO 32
#define RESTART 64

extern volatile unsigned int ticks, missed;
extern volatile unsigned char ready;
extern unsigned char stage_d018, stage_dx, stage_dy;
extern unsigned char sx[8],sy[8],sptr[8],scol[8],xmask,enable;
extern unsigned char *view_ptr;
void engine_init(void);
unsigned char read_input(void);
unsigned int clock_read(void);
void render_a(void);
void render_b(void);
void fast_physics(void);
void fast_actors(void);
void project_sprites(void);
unsigned char demo_input(void);
void trace_frame(void);
void fast_camera(void);
void fast_interact(void);
extern unsigned char effect,needs_hurt;

unsigned int camera_x,camera_y;
unsigned int px,py,pyq,checkpoint_x,checkpoint_y;
signed char vy;
unsigned char grounded,facing,state,demo,route,health,signals;
unsigned char invulnerable,fire_delay,keys,oldkeys,dirty,front,warmup;
unsigned char coarse_x,coarse_y,last_diag,phase,sfx_left;
unsigned int updates,worst,last_cost,renders,score,mode_age,started_at;
unsigned int shot_x,shot_y;
unsigned char shot_life,shot_left;
unsigned int ex[4],ey[4];
unsigned char alive[4];
const unsigned int enemy_x[4]={316,484,742,930};
const unsigned int enemy_y[4]={354,286,224,136};
const unsigned int relay_x[3]={224,544,896};
const unsigned int relay_y[3]={360,264,168};
const unsigned int route_x[7]={128,224,360,544,704,896,968};
const unsigned int route_y[7]={412,364,316,268,220,172,124};
const signed char dir_x[8]={1,1,0,-1,-1,-1,0,1};
const signed char dir_y[8]={0,1,1,1,0,-1,-1,-1};
const unsigned int notes[8]={2195,2766,3289,2766,1956,2464,2930,2464};


static void text(unsigned char x,unsigned char y,const char *s,unsigned char color) {
    unsigned int p=(unsigned int)y*40+x;
    while(*s) {SCREEN[p]=(*s++)&63;COLOR[p++]=color;}
}
static void number(unsigned char x,unsigned char y,unsigned int value) {
    char buf[6];unsigned char i;
    buf[5]=0;
    for(i=5;i;i--) {buf[i-1]='0'+value%10;value/=10;}
    text(x,y,buf,1);
}
static void screen_message(void) {
    unsigned int i;unsigned char x,y;
    memset(SCREEN,0,1000);memset(COLOR,1,1000);
    for(i=0;i<40;i++) {SCREEN[40+i]=128;SCREEN[920+i]=128;COLOR[40+i]=6;COLOR[920+i]=6;}
    stage_d018=2;stage_dx=8;stage_dy=23;enable=0;front=0;
    for(y=0;y<2;y++) for(x=0;x<22;x++) {i=(unsigned int)(4+y)*40+9+x;SCREEN[i]=208+y*22+x;COLOR[i]=3;}
    if(state==0) {
        text(9,8,"THE LAST RELAY IS STILL ALIVE",8);
        text(6,11,"CLIMB THE ABANDONED STATION",1);
        text(6,13,"LINK THREE RELAYS. REACH THE EXIT.",3);
        text(6,16,"A/D MOVE  W JUMP  SPACE FIRE",1);
        text(6,18,"S LINK/DROP/EXIT   JOY PORT 2",1);
        text(6,21,"FIRE TO START    F1 FOR DEMO",7);
    } else if(state==2) {
        text(12,10,"CONNECTION LOST",2);
        text(8,13,"THE STATION CLAIMED ANOTHER",1);
        text(12,16,"SCORE",3);number(21,16,score);
        text(8,21,"FIRE TO RETRY   F1 DEMO",7);
    } else {
        text(11,10,"SIGNAL RESTORED",5);
        text(7,13,"ALL THREE RELAYS ARE ONLINE",1);
        text(9,15,"THE SURFACE HEARD YOUR CALL",3);
        text(10,18,"SCORE",3);number(20,18,score);
        text(7,21,"MISSION COMPLETE - FIRE AGAIN",7);
    }
    mode_age=0;
}
static void sound(unsigned char kind) {
    POKE(0xd412,0);
    POKE(0xd40e,kind==1?180:40);POKE(0xd40f,kind==1?52:12);
    POKE(0xd413,0x04);POKE(0xd414,0x88);POKE(0xd412,kind==3?0x81:0x21);
    sfx_left=kind==3?12:5;
}
static void music(void) {
    unsigned int n;
    if((phase&15)==0) {
        n=notes[(phase>>4)&7];
        POKE(0xd400,(unsigned char)n);POKE(0xd401,n>>8);
        POKE(0xd404,0x11);
        n>>=1;POKE(0xd407,(unsigned char)n);POKE(0xd408,n>>8);POKE(0xd40b,0x11);
    }
    if(sfx_left && --sfx_left==0) POKE(0xd412,0);
}
static void relay_art(unsigned char which,unsigned char active) {
    unsigned char *p=RAM+((relay_y[which]>>3)<<7)+(relay_x[which]>>3);
    unsigned char b=active?86:80;
    p[0]=b;p[1]=b+1;p[128]=b+2;p[129]=b+3;p[256]=b+4;p[257]=b+5;
    dirty=1;
}

static void new_game(void) {
    unsigned char i;
    px=64;py=460;pyq=py<<2;vy=0;grounded=0;facing=0;
    checkpoint_x=px;checkpoint_y=py;
    health=5;signals=0;route=0;invulnerable=90;
    shot_life=0;fire_delay=0;score=0;phase=0;front=0;
    camera_x=0;camera_y=312;coarse_x=255;coarse_y=255;dirty=1;
    last_diag=0;renders=0;updates=0;worst=0;missed=0;warmup=8;
    for(i=0;i<4;i++) {ex[i]=enemy_x[i];ey[i]=enemy_y[i];alive[i]=1;}
    for(i=0;i<3;i++) relay_art(i,0);
    memset(COLOR,11,1000);
    stage_dx=23;stage_dy=23;stage_d018=2;state=1;mode_age=0;
    started_at=ticks;
}
static void hurt(void) {
    if(invulnerable) return;
    if(health) --health;
    sound(3);invulnerable=90;
    if(!health) {state=2;screen_message();return;}
    px=checkpoint_x;py=checkpoint_y;pyq=py<<2;vy=0;grounded=0;
    route=signals==7?6:(signals==3?4:(signals==1?2:0));
}

static void actors(void) {
    fast_actors();
    if(needs_hurt) hurt();
    if(state!=1) return;
    fast_interact();
    if(signals==7 && (keys&DOWN) && px>952 && py<150) {score+=500;state=3;screen_message();}
}
static void display(void) {
    fast_camera();
    project_sprites();
}

static void telemetry(void) {
    POKE(0x0340,68);POKE(0x0341,83);POKE(0x0342,1);POKE(0x0343,state);
    POKEW(0x0344,ticks);POKEW(0x0346,updates);POKEW(0x0348,missed);
    POKEW(0x034a,worst);POKEW(0x034c,renders);
    POKEW(0x034e,px);POKEW(0x0350,py);
    POKEW(0x0352,camera_x);POKEW(0x0354,camera_y);
    POKE(0x0356,health);POKE(0x0357,signals);POKE(0x0358,demo);
    POKE(0x0359,enable);POKE(0x035a,coarse_x);POKE(0x035b,coarse_y);
    POKEW(0x035c,last_cost);POKE(0x035e,route);POKE(0x035f,grounded);
}
void main(void) {
    unsigned int begin;
    unsigned char hardware;
    POKE(INPUT_PULSE,0);POKE(INPUT_HOLD,0);POKE(DIAGNOSTIC,0);POKE(0x033d,0);
    POKE(0xd020,0);POKE(0xd021,0);POKE(0xd022,11);POKE(0xd023,6);
    POKE(0xd418,10);POKE(0xd405,0x49);POKE(0xd406,0x65);
    POKE(0xd40c,0x69);POKE(0xd40d,0x43);
    state=0;screen_message();ready=1;engine_init();
    for(;;) {
        while(ready) {}
        begin=clock_read();hardware=read_input();keys=hardware;++mode_age;++phase;
        if(state!=1) {
            if((hardware&(FIRE|DEMO)) || (state==0 && mode_age>500) || (state==2 && demo && mode_age>125)) {
                demo=(hardware&DEMO)?1:((hardware&FIRE)?0:1);new_game();
            }
        } else {
            if((hardware&DEMO) && !(oldkeys&DEMO)) demo^=1;
            if(hardware&RESTART) {demo=0;new_game();}
            if(demo) keys=demo_input();
            if(invulnerable) --invulnerable;
            fast_physics();
            if(needs_hurt) hurt();
            if(state==1) actors();
        }
        if(state==1) display();
        if(effect) {sound(effect);effect=0;}
        music();oldkeys=hardware;if(state==1) ++updates;
        last_cost=begin-clock_read();
        if(warmup) {--warmup;worst=0;missed=0;updates=0;}
        else if(state==1 && last_cost>worst) worst=last_cost;
        if(PEEK(0x033d)) trace_frame();
        telemetry();ready=1;
    }
}
