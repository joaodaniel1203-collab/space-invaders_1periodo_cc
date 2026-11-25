/*
  TP Allegro - Space Invaders	
 
  Organizacao do arquivo:
    1. Constantes e tipos (structs)
    2. Utilitarios (colisao, posicao das naves)
    3. Canhao   (init / update / draw)
    4. Tiro     (init / disparar / update / draw)
    5. Enxame   (init / update / draw)  -> o grupo de aliens
    6. Regras do jogo (colisao do tiro, fim de jogo, recorde)
   7. Fundo de estrelas
    8. Telas (cenario, HUD, inicio, derrota, vitoria)
    9. main: inicializacao + laco de eventos


  
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>

/* ------------------------------------------------------------------ */
// 1. CONSTANTES E TIPOS                                              
/* ------------------------------------------------------------------ */


#define N_LIN 4                 
#define N_COL 7                 
#define ARQ_RECORDE "recorde.txt"

const int SCREEN_W = 960;
const int SCREEN_H = 540;
const int GRASS_H  = 60;
const float FPS    = 100;

/* canhao */
const int   CANHAO_H   = 70;
const int   CANHAO_W   = 70;
const float CANHAO_VEL = 3;

/* naves alienigenas */
const int   ALIEN_W        = 40;
const int   ALIEN_H        = 30;
const int   ALIEN_GAP      = 30;    /* espaco entre naves */
const int   ALIEN_TOPO_Y   = 50;    /* y inicial do grupo (abaixo do placar)  */
const float ALIEN_SALTO    = 25;    /* quanto desce ao bater na borda         */
const float ALIEN_VEL_BASE = 0.8;   /* pixels por tick no inicio              */
const float ALIEN_VEL_INC  = 0.04;  /* aceleracao a cada nave destruida       */

/* tiro */
const int   TIRO_W   = 4;
const int   TIRO_H   = 14;
const float TIRO_VEL = 5;

/* distancia de seguranca: nave "muito proxima" do canhao encerra o jogo */
const int DIST_SEG = 3;

/* volumes dos sons (0.0 = mudo, 1.0 = normal) */
const float VOL_TIRO     = 0.5;
const float VOL_EXPLOSAO = 0.5;
const float VOL_MARCHA   = 0.4;
const float VOL_FIM      = 0.8;
const float VOL_INICIO   = 0.6;
const float VOL_COMECAR  = 0.8;

typedef struct Canhao {
	float x;                 /* centro do canhao no eixo x */
	float vel;
	int dir, esq;            /* 1 enquanto a tecla esta pressionada */
	ALLEGRO_COLOR cor;
} Canhao;

typedef struct Tiro {
	float x, y;              /* y = topo do tiro */
	float vel;
	int ativo;               /* 1 = ha um tiro no ar */
	ALLEGRO_COLOR cor;
} Tiro;

/*
 * O enxame guarda SO a posicao do grupo (x, y do canto superior esquerdo)
 * e quais naves estao vivas. A posicao de cada nave e CALCULADA a partir
 * da linha/coluna dela (ver alien_x / alien_y). Assim, mover o grupo inteiro
 * e apenas somar em e->x, e as naves sempre andam juntas.
 */
typedef struct Enxame {
	float x, y;
	int dir;                 /* +1 = direita, -1 = esquerda */
	float vel;
	int vivo[N_LIN][N_COL];  /* 1 = viva, 0 = destruida */
	int restantes;
} Enxame;

typedef enum Estado { INICIO, JOGANDO, VITORIA, DERROTA } Estado;

#define NUM_CAMADAS 3
#define ESTRELAS_POR_CAMADA 55

typedef struct Estrela {
	float x, y;
	float velocidade_y;
	float velocidade_x;
	int tamanho;
} Estrela;

Estrela estrelas[NUM_CAMADAS][ESTRELAS_POR_CAMADA];

/* ------------------------------------------------------------------ */
// 2. UTILITARIOS                                                     */
/* ------------------------------------------------------------------ */

float alien_x(const Enxame *e, int col){
	return e->x + col * (ALIEN_W + ALIEN_GAP);
}

float alien_y(const Enxame *e, int lin){
	return e->y + lin * (ALIEN_H + ALIEN_GAP);
}

/* Colisao entre dois retangulos (AABB): (x, y, largura, altura) cada um.
 * Eles colidem se se sobrepoem NOS DOIS eixos ao mesmo tempo. */
int colide(float ax, float ay, float aw, float ah,
           float bx, float by, float bw, float bh){
	return ax < bx + bw && ax + aw > bx &&
	       ay < by + bh && ay + ah > by;
}

/* y da base do canhao (ele fica "em pe" no meio da faixa de grama) */
float canhao_base_y(void){
	return SCREEN_H - GRASS_H / 2;
}

/* ------------------------------------------------------------------ */
// 3. CANHAO                                                          */
/* ------------------------------------------------------------------ */

void init_canhao(Canhao *c){
	c->x   = SCREEN_W / 2;
	c->vel = CANHAO_VEL;
	c->dir = 0;
	c->esq = 0;
	c->cor = al_map_rgb(0, 0, 255);
}

void update_canhao(Canhao *c){
	if (c->dir) c->x += c->vel;
	if (c->esq) c->x -= c->vel;

	/* "clamp": se passou do limite, prende na borda (considera a metade da largura) */
	if (c->x - CANHAO_W / 2 < 0)        c->x = CANHAO_W / 2;
	if (c->x + CANHAO_W / 2 > SCREEN_W) c->x = SCREEN_W - CANHAO_W / 2;
}

void draw_canhao(const Canhao *c, ALLEGRO_BITMAP *img){
	float y_base = canhao_base_y();
	al_draw_scaled_bitmap(img, 0, 0,
	                      al_get_bitmap_width(img), al_get_bitmap_height(img),
	                      c->x - CANHAO_W / 2, y_base - CANHAO_H,
	                      CANHAO_W, CANHAO_H, 0);
}

/* ------------------------------------------------------------------ */
//4. TIRO                                                            */
/* ------------------------------------------------------------------ */

void init_tiro(Tiro *t){
	t->x = 0;
	t->y = 0;
	t->vel = TIRO_VEL;
	t->ativo = 0;
	t->cor = al_map_rgb(255, 255, 0);
}

/* So dispara se NAO houver tiro no ar */
void disparar(Tiro *t, const Canhao *c){
	if (t->ativo) return;
	t->ativo = 1;
	t->x = c->x;                              /* sai da ponta do canhao ... */
	t->y = canhao_base_y() - CANHAO_H - TIRO_H;
}

void update_tiro(Tiro *t){
	if (!t->ativo) return;
	t->y -= t->vel;                           /* so o y muda: tiro 100% vertical */
	if (t->y + TIRO_H < 0)                    /* saiu inteiro pelo topo */
		t->ativo = 0;                         /* libera o proximo disparo */
}

void draw_tiro(const Tiro *t){
	if (!t->ativo) return;
	al_draw_filled_rectangle(t->x - TIRO_W / 2, t->y,
	                         t->x + TIRO_W / 2, t->y + TIRO_H, t->cor);
}

/* ------------------------------------------------------------------ */
//5. ENXAME (grupo de aliens)                                        */
/* ------------------------------------------------------------------ */

void init_enxame(Enxame *e){
	int lin, col;
	int largura = N_COL * ALIEN_W + (N_COL - 1) * ALIEN_GAP;

	e->x = (SCREEN_W - largura) / 2;          /* centraliza o grupo */
	e->y = ALIEN_TOPO_Y;
	e->dir = 1;
	e->vel = ALIEN_VEL_BASE;
	e->restantes = N_LIN * N_COL;

	for (lin = 0; lin < N_LIN; lin++)
		for (col = 0; col < N_COL; col++)
			e->vivo[lin][col] = 1;
}

/* 1 se existe pelo menos uma nave viva na coluna */
int coluna_viva(const Enxame *e, int col){
	int lin;
	for (lin = 0; lin < N_LIN; lin++)
		if (e->vivo[lin][col]) return 1;
	return 0;
}

void update_enxame(Enxame *e){
	int col, col_esq = -1, col_dir = -1;
	float dx, borda_esq, borda_dir;

	/* acha a coluna viva mais a esquerda e a mais a direita:
	 * se uma coluna inteira morreu, o grupo continua ate a borda */
	for (col = 0; col < N_COL; col++){
		if (coluna_viva(e, col)){
			if (col_esq == -1) col_esq = col;
			col_dir = col;
		}
	}
	if (col_esq == -1) return;                /* nao ha naves vivas */

	/* quanto menos naves, mais rapido */
	e->vel = ALIEN_VEL_BASE + (N_LIN * N_COL - e->restantes) * ALIEN_VEL_INC;

	dx = e->dir * e->vel;
	borda_esq = alien_x(e, col_esq) + dx;
	borda_dir = alien_x(e, col_dir) + ALIEN_W + dx;

	if (borda_esq < 0 || borda_dir > SCREEN_W){
		e->y  += ALIEN_SALTO;                 /* salto em direcao ao solo */
		e->dir = -e->dir;                     /* inverte a direcao */
	} else {
		e->x += dx;                           /* anda todo mundo junto */
	}
}

ALLEGRO_COLOR cor_da_linha(int lin){
	switch (lin){
		case 0:  return al_map_rgb(rand()%255, 255,rand()%255);   
		case 1:  return al_map_rgb(rand()%255, 255, rand()%255);  
		case 2:  return al_map_rgb(rand()%255, 255,rand()%255);   
		default: return al_map_rgb(rand()%255, 255, rand()%255);   
	}
}

void draw_alien(float x, float y, ALLEGRO_COLOR cor){
	ALLEGRO_COLOR preto = al_map_rgb(0, 0, 0);

	al_draw_filled_rectangle(x + 8, y, x + ALIEN_W - 8, y + 8, cor);                    /* cabeca */
	al_draw_filled_rectangle(x, y + 8, x + ALIEN_W, y + ALIEN_H - 8, cor);              /* corpo  */
	al_draw_filled_rectangle(x + 10, y + 12, x + 16, y + 18, preto);                    /* olho esq */
	al_draw_filled_rectangle(x + ALIEN_W - 16, y + 12, x + ALIEN_W - 10, y + 18, preto);/* olho dir */
	al_draw_filled_rectangle(x + 6, y + ALIEN_H - 8, x + 12, y + ALIEN_H, cor);         /* perna esq */
	al_draw_filled_rectangle(x + ALIEN_W - 12, y + ALIEN_H - 8, x + ALIEN_W - 6, y + ALIEN_H, cor); /* perna dir */
}

void draw_enxame(const Enxame *e){
	int lin, col;
	for (lin = 0; lin < N_LIN; lin++)
		for (col = 0; col < N_COL; col++)
			if (e->vivo[lin][col])            /* nave destruida nao e desenhada */
				draw_alien(alien_x(e, col), alien_y(e, lin), cor_da_linha(lin));
}

/* ------------------------------------------------------------------ */
// 6. REGRAS DO JOGO                                                  */
/* ------------------------------------------------------------------ */

/* Testa o tiro contra cada nave viva. Retorna os pontos ganhos (0 se errou).
 * Linhas mais altas (mais longe) valem mais, como no original. */
int tiro_acerta(Tiro *t, Enxame *e){
	int lin, col;
	if (!t->ativo) return 0;

	for (lin = 0; lin < N_LIN; lin++){
		for (col = 0; col < N_COL; col++){
			if (!e->vivo[lin][col]) continue;
			if (colide(t->x - TIRO_W / 2, t->y, TIRO_W, TIRO_H,
			           alien_x(e, col), alien_y(e, lin), ALIEN_W, ALIEN_H)){
				e->vivo[lin][col] = 0;        /* nave destruida */
				e->restantes--;
				t->ativo = 0;                 /* o tiro some junto */
				return (N_LIN - lin) * 10;
			}
		}
	}
	return 0;
}

/* Decide se o jogo continua, foi vencido ou perdido */
Estado checar_fim(const Enxame *e, const Canhao *c){
	int lin, col;
	float ay;

	if (e->restantes == 0) return VITORIA;

	/* retangulo do canhao ampliado pela distancia de seguranca */
	float cx = c->x - CANHAO_W / 2 - DIST_SEG;
	float cy = canhao_base_y() - CANHAO_H - DIST_SEG;
	float cw = CANHAO_W + 2 * DIST_SEG;
	float ch = CANHAO_H + 2 * DIST_SEG;

	for (lin = 0; lin < N_LIN; lin++){
		for (col = 0; col < N_COL; col++){
			if (!e->vivo[lin][col]) continue;
			ay = alien_y(e, lin);

			if (ay + ALIEN_H >= SCREEN_H - GRASS_H)               /* tocou o solo */
				return DERROTA;
			if (colide(alien_x(e, col), ay, ALIEN_W, ALIEN_H,     /* tocou/perto do canhao */
			           cx, cy, cw, ch))
				return DERROTA;
		}
	}
	return JOGANDO;
}

/* Recorde em arquivo texto: um inteiro. Se o arquivo nao existe, recorde = 0 */
int ler_recorde(void){
	int r = 0;
	FILE *f = fopen(ARQ_RECORDE, "r");
	if (f){
		if (fscanf(f, "%d", &r) != 1) r = 0;
		fclose(f);
	}
	return r;
}

void salvar_recorde(int r){
	FILE *f = fopen(ARQ_RECORDE, "w");
	if (f){
		fprintf(f, "%d\n", r);
		fclose(f);
	}
}

/* ------------------------------------------------------------------ */
//7. FUNDO DE ESTRELAS (parallax)                                    */
/* ------------------------------------------------------------------ */

void init_estrelas(void){
	int camada, i;

	for (camada = 0; camada < NUM_CAMADAS; camada++){
		for (i = 0; i < ESTRELAS_POR_CAMADA; i++){
			Estrela *e = &estrelas[camada][i];
			e->x = (float)(rand() % SCREEN_W);
			e->y = (float)(rand() % SCREEN_H);

			if (camada == 0){
				e->velocidade_y = 12.0f;
				e->velocidade_x = 1.5f;
				e->tamanho = 1;
			} else if (camada == 1){
				e->velocidade_y = 32.0f;
				e->velocidade_x = 4.0f;
				e->tamanho = 2;
			} else {
				e->velocidade_y = 65.0f;
				e->velocidade_x = 8.0f;
				e->tamanho = 3;
			}
		}
	}
}

void update_estrelas(float dt){
	int camada, i;

	for (camada = 0; camada < NUM_CAMADAS; camada++){
		for (i = 0; i < ESTRELAS_POR_CAMADA; i++){
			Estrela *e = &estrelas[camada][i];
			e->y += e->velocidade_y * dt;
			e->x += e->velocidade_x * dt;

			if (e->y >= SCREEN_H){
				e->y = 0.0f;
				e->x = (float)(rand() % SCREEN_W);
			}
			if (e->x >= SCREEN_W) e->x = 0.0f;
		}
	}
}

void draw_estrelas(void){
	int camada, i;

	for (camada = 0; camada < NUM_CAMADAS; camada++){
		for (i = 0; i < ESTRELAS_POR_CAMADA; i++){
			Estrela *e = &estrelas[camada][i];
			ALLEGRO_COLOR cor;

			if (camada == 0)
				cor = al_map_rgb(95, 70, 140);
			else if (camada == 1)
				cor = al_map_rgb(165, 125, 215);
			else
				cor = al_map_rgb(235, 220, 255);

			al_draw_filled_rectangle(e->x, e->y,
			                         e->x + e->tamanho,
			                         e->y + e->tamanho, cor);

			/* estrelas proximas recebem pequeno brilho em cruz. */
			if (camada == 2 && i % 9 == 0){
				al_draw_filled_rectangle(e->x - 2, e->y + 1,
				                         e->x + e->tamanho + 2, e->y + 2,
				                         al_map_rgb(190, 155, 235));
				al_draw_filled_rectangle(e->x + 1, e->y - 2,
				                         e->x + 2, e->y + e->tamanho + 2,
				                         al_map_rgb(190, 155, 235));
			}
		}
	}
}

/* ------------------------------------------------------------------ */
/* 8. TELAS                                                           */
/* ------------------------------------------------------------------ */

void draw_scenario(ALLEGRO_BITMAP *fundo){
	al_clear_to_color(al_map_rgb(0, 0, 0));
	al_draw_scaled_bitmap(fundo, 0, 0,
	                      al_get_bitmap_width(fundo), al_get_bitmap_height(fundo),
	                      0, 0, SCREEN_W, SCREEN_H, 0);
	draw_estrelas();
}

void draw_hud(ALLEGRO_FONT *fonte, int pontos, int recorde){
	ALLEGRO_COLOR branco = al_map_rgb(255, 255, 255);
	al_draw_textf(fonte, branco, 10, 10, ALLEGRO_ALIGN_LEFT, "PONTOS: %d", pontos);
	al_draw_textf(fonte, branco, SCREEN_W - 10, 10, ALLEGRO_ALIGN_RIGHT, "RECORDE: %d", recorde);
	
}



/* Desenha o texto UMA vez dentro de um bitmap (fundo transparente).
 * Depois e so desenhar o bitmap: 1 chamada por texto em vez de centenas. */
 

	void draw_inicio (ALLEGRO_BITMAP *fundo, ALLEGRO_FONT *grande,ALLEGRO_FONT *pequeno,int ticks){
		int cx = SCREEN_W/2;
		int right = SCREEN_W - 20;
		ALLEGRO_COLOR branco = al_map_rgb(235, 235, 235);
		
		draw_scenario(fundo);
		al_draw_filled_rectangle(0,0, SCREEN_W, SCREEN_H, al_map_rgba(0, 0, 0, 150));//rbg(a)->alpha transparence
		al_draw_text(grande, branco, cx, 70, ALLEGRO_ALIGN_CENTRE, "SPACE INVADERS");
			

	if ((ticks / 50) % 2 == 0)
		al_draw_text(pequeno, al_map_rgb(90, 255, 120), cx, 300, ALLEGRO_ALIGN_CENTER, "PRESSIONE ENTER");
	al_draw_text(pequeno, al_map_rgb(150, 150, 150), cx, 425, ALLEGRO_ALIGN_CENTRE,
	             "A D OU SETAS: MOVER    ESPACO: ATIRAR");
	al_draw_text(pequeno, al_map_rgba(100, 150, 150, 100),right, 490, ALLEGRO_ALIGN_RIGHT,
	             "Joao Daniel");
				 		 
				 
	}





/* Tela de derrota: cor vermelha sobre o cenario */
void draw_derrota(ALLEGRO_FONT *fonte, ALLEGRO_FONT *titulo,
                  int pontos, int recorde, int novo_recorde, int ticks){
	int cx = SCREEN_W / 2;
	float lh = al_get_font_line_height(fonte) + 10;
	float y = 90;
	ALLEGRO_COLOR branco = al_map_rgb(255, 255, 255);

	al_draw_filled_rectangle(0, 0, SCREEN_W, SCREEN_H, al_map_rgba(120, 0, 0, 150));

	al_draw_text(titulo, al_map_rgb(255, 60, 60), cx, y, ALLEGRO_ALIGN_CENTRE, "GAME OVER");
	y += al_get_font_line_height(titulo) + 10;
	al_draw_text(fonte, branco, cx, y, ALLEGRO_ALIGN_CENTRE, "A TERRA FOI INVADIDA!");

	y += lh * 2;
	al_draw_textf(fonte, branco, cx, y, ALLEGRO_ALIGN_CENTRE, "Sua pontuacao: %d", pontos);
	y += lh;
	al_draw_textf(fonte, branco, cx, y, ALLEGRO_ALIGN_CENTRE, "Recorde atual: %d", recorde);
	if (novo_recorde){
		y += lh;
		al_draw_text(fonte, al_map_rgb(255, 255, 0), cx, y, ALLEGRO_ALIGN_CENTRE, "NOVO RECORDE!");
	}

	y += lh * 2;
	al_draw_text(fonte, al_map_rgb(255, 220, 120), cx, y, ALLEGRO_ALIGN_CENTRE,
	             "Nao desista! A Terra precisa de voce!");
	y += lh;
	if ((ticks / 50) % 2 == 0)
		al_draw_text(fonte, branco, cx, y, ALLEGRO_ALIGN_CENTRE, "ENTER = tentar de novo    ESC = sair");
}

/* Tela de vitoria */
void draw_fim(ALLEGRO_FONT *fonte, Estado estado, int pontos, int recorde, int novo_recorde){
	int cx = SCREEN_W / 2;
	float lh = al_get_font_line_height(fonte) + 10;
	float y = 110;
	ALLEGRO_COLOR branco = al_map_rgb(255, 255, 255);

	if (estado == VITORIA)
		al_draw_text(fonte, al_map_rgb(0, 255, 0), cx, y, ALLEGRO_ALIGN_CENTRE, "VOCE SALVOU A TERRA!");
	else
		al_draw_text(fonte, al_map_rgb(255, 60, 60), cx, y, ALLEGRO_ALIGN_CENTRE, "A TERRA FOI INVADIDA!");

	y += lh * 2;
	al_draw_textf(fonte, branco, cx, y, ALLEGRO_ALIGN_CENTRE, "Sua pontuacao: %d", pontos);
	y += lh;
	al_draw_textf(fonte, branco, cx, y, ALLEGRO_ALIGN_CENTRE, "Recorde atual: %d", recorde);

	if (novo_recorde){
		y += lh;
		al_draw_text(fonte, al_map_rgb(255, 255, 0), cx, y, ALLEGRO_ALIGN_CENTRE, "NOVO RECORDE!");
	}

	y += lh * 2;
	al_draw_text(fonte, branco, cx, y, ALLEGRO_ALIGN_CENTRE, "ENTER = jogar de novo    ESC = sair");
	al_draw_text(fonte, al_map_rgba(100, 150, 150, 100),SCREEN_W-20, 490, ALLEGRO_ALIGN_RIGHT,
	             "Joao Daniel");
				 		 
}

/* ------------------------------------------------------------------ */
/* 9. MAIN                                                            */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv){

	ALLEGRO_DISPLAY *display = NULL;
	ALLEGRO_EVENT_QUEUE *event_queue = NULL;
	ALLEGRO_TIMER *timer = NULL;
	ALLEGRO_FONT *fonte = NULL;
	ALLEGRO_FONT *titulo = NULL;
	int i;

	/* --- inicializacao --- */
	srand((unsigned int)time(NULL));
	if (!al_init()){
		fprintf(stderr, "failed to initialize allegro!\n");
		return -1;
	}
	if (!al_init_primitives_addon()){
		fprintf(stderr, "failed to initialize primitives!\n");
		return -1;
	}
	al_init_font_addon();
	if (!al_init_ttf_addon()){
		fprintf(stderr, "failed to load ttf font module!\n");
		return -1;
	}
	if (!al_init_image_addon()){
		fprintf(stderr, "falha ao inicializar o addon de imagens\n");
		return -1;
	}
	if (!al_install_audio() || !al_init_acodec_addon()){
		fprintf(stderr, "failed to initialize audio!\n");
		return -1;
	}
	al_reserve_samples(8);               /* quantos sons podem tocar ao mesmo tempo */

	/* o display precisa existir ANTES de carregar imagens (senao fica lento) */
	display = al_create_display(SCREEN_W, SCREEN_H);
	if (!display){
		fprintf(stderr, "failed to create display!\n");
		return -1;
	}
	timer = al_create_timer(1.0 / FPS);
	if (!timer){
		fprintf(stderr, "failed to create timer!\n");
		return -1;
	}
	if (!al_install_keyboard()){
		fprintf(stderr, "failed to install keyboard!\n");
		return -1;
	}

	/* --- imagens --- */
	ALLEGRO_BITMAP *fundo = al_load_bitmap("imagens/fundo.png");
	if (!fundo){
		fprintf(stderr, "nao foi possivel carregar imagens/fundo.png\n");
		return -1;
	}
	ALLEGRO_BITMAP *img_canhao = al_load_bitmap("imagens/canhao.png");
	if (!img_canhao){
		fprintf(stderr, "nao foi possivel carregar imagens/canhao.png\n");
		return -1;
	}

	/* --- sons --- */
	ALLEGRO_SAMPLE *som_tiro     = al_load_sample("sons/tiro.wav");
	ALLEGRO_SAMPLE *som_explosao = al_load_sample("sons/explosao.wav");
	ALLEGRO_SAMPLE *som_derrota  = al_load_sample("sons/derrota.wav");
	ALLEGRO_SAMPLE *som_vitoria  = al_load_sample("sons/vitoria.wav");
	ALLEGRO_SAMPLE *som_inicio   = al_load_sample("sons/inicio.wav");
	ALLEGRO_SAMPLE *som_comecar  = al_load_sample("sons/comecar.wav");
	ALLEGRO_SAMPLE *som_marcha[4];
	som_marcha[0] = al_load_sample("sons/marcha1.wav");
	som_marcha[1] = al_load_sample("sons/marcha2.wav");
	som_marcha[2] = al_load_sample("sons/marcha3.wav");
	som_marcha[3] = al_load_sample("sons/marcha4.wav");

	if (!som_tiro || !som_explosao || !som_derrota || !som_vitoria ||
	    !som_inicio || !som_comecar ||
	    !som_marcha[0] || !som_marcha[1] || !som_marcha[2] || !som_marcha[3]){
		fprintf(stderr, "falha ao carregar os sons (pasta sons/ esta ao lado do .exe?)\n");
		return -1;
	}

	/* --- fontes: tenta a TTF; se nao achar, usa a fonte embutida --- */
	fonte = al_load_font("fonts/pixel.ttf", 24, 0);
	if (!fonte){
		fprintf(stderr, "fonts/pixel.ttf nao encontrada, usando fonte padrao\n");
		fonte = al_create_builtin_font();
		if (!fonte) return -1;
	}
	titulo = al_load_font("fonts/PressStart2P-Regular.ttf", 64, 0);
	if (!titulo) titulo = fonte;  
		ALLEGRO_FONT *pixel_g = al_load_font("fonts/pixel.ttf", 48, ALLEGRO_TTF_MONOCHROME);
		ALLEGRO_FONT *pixel_p = al_load_font("/fonts/pixel.ttf", 16, ALLEGRO_TTF_MONOCHROME);
	if (!pixel_g || !pixel_p){
		fprintf(stderr, "fonts/PressStart2P-Regular.ttf nao encontrada, usando arial\n");
	if (pixel_g) al_destroy_font(pixel_g);
	if (pixel_p) al_destroy_font(pixel_p);
	pixel_g = titulo;
	pixel_p = fonte;
	}	/* sem arial.ttf, reaproveita a fonte pequena */

	

	event_queue = al_create_event_queue();
	if (!event_queue){
		fprintf(stderr, "failed to create event_queue!\n");
		return -1;
	}
	al_register_event_source(event_queue, al_get_display_event_source(display));
	al_register_event_source(event_queue, al_get_keyboard_event_source());
	al_register_event_source(event_queue, al_get_timer_event_source(timer));

	/* --- estado do jogo --- */
	Canhao canhao;
	Tiro tiro;
	Enxame enxame;
	Estado estado = INICIO;              /* o jogo abre na tela de inicio */
	int pontos = 0;
	int recorde = ler_recorde();
	int novo_recorde = 0;

	int ticks = 0;                       /* conta ticks: base das animacoes */
	int ticks_marcha = 0, nota = 0;      /* controle da "marcha" das naves */
	int som_fim_tocado = 0;              /* trava: toca o som de fim uma vez so */
	int redraw = 0;
	ALLEGRO_SAMPLE_ID id_musica;         /* para poder parar a musica do inicio */

	init_canhao(&canhao);
	init_tiro(&tiro);
	init_enxame(&enxame);
	init_estrelas();

	al_start_timer(timer);

	/* musica da tela de inicio, em loop */
	al_play_sample(som_inicio, VOL_INICIO, 0.0, 1.0, ALLEGRO_PLAYMODE_LOOP, &id_musica);

	/* --- laco principal --- */
	int playing = 1;
	while (playing){
		ALLEGRO_EVENT ev;
		al_wait_for_event(event_queue, &ev);

		if (ev.type == ALLEGRO_EVENT_TIMER){
			ticks++;
			redraw = 1;

			/* o universo continua se movendo em todas as telas */
			update_estrelas(1.0f / FPS);

			/* atualiza a logica (so enquanto estiver jogando) */
			if (estado == JOGANDO){
				update_canhao(&canhao);
				update_tiro(&tiro);
				update_enxame(&enxame);

				int ganho = tiro_acerta(&tiro, &enxame);
				if (ganho > 0)
					al_play_sample(som_explosao, VOL_EXPLOSAO, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
				pontos += ganho;

				/* marcha: 4 notas em sequencia, mais rapido com menos naves */
				ticks_marcha++;
				if (ticks_marcha >= 20 + enxame.restantes * 2){
					ticks_marcha = 0;
					al_play_sample(som_marcha[nota], VOL_MARCHA, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
					nota = (nota + 1) % 4;
				}

				estado = checar_fim(&enxame, &canhao);

				if (estado != JOGANDO){          /* acabou de terminar */
					novo_recorde = pontos > recorde;
					if (novo_recorde){
						recorde = pontos;
						salvar_recorde(recorde);
					}
				}
			}
		}
		else if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE){
			playing = 0;
		}
		else if (ev.type == ALLEGRO_EVENT_KEY_DOWN){
			switch (ev.keyboard.keycode){
				case ALLEGRO_KEY_A:
				case ALLEGRO_KEY_LEFT:
					canhao.esq = 1;
					break;
				case ALLEGRO_KEY_D:
				case ALLEGRO_KEY_RIGHT:
					canhao.dir = 1;
					break;
				case ALLEGRO_KEY_SPACE:
					if (estado == JOGANDO && !tiro.ativo){
						disparar(&tiro, &canhao);
						al_play_sample(som_tiro, VOL_TIRO, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
					}
					break;
				case ALLEGRO_KEY_ENTER:
					if (estado == INICIO){
						al_stop_sample(&id_musica);          /* para a musica de abertura */
						al_play_sample(som_comecar, VOL_COMECAR, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
						estado = JOGANDO;
					} else if (estado != JOGANDO){           /* reinicia a partida */
						init_canhao(&canhao);
						init_tiro(&tiro);
						init_enxame(&enxame);
						pontos = 0;
						novo_recorde = 0;
						ticks_marcha = 0;
						nota = 0;
						som_fim_tocado = 0;
						estado = JOGANDO;
					}
					break;
				case ALLEGRO_KEY_ESCAPE:
					playing = 0;
					break;
			}
		}
		else if (ev.type == ALLEGRO_EVENT_KEY_UP){
			switch (ev.keyboard.keycode){
				case ALLEGRO_KEY_A:
				case ALLEGRO_KEY_LEFT:
					canhao.esq = 0;
					break;
				case ALLEGRO_KEY_D:
				case ALLEGRO_KEY_RIGHT:
					canhao.dir = 0;
					break;
			}
		}

		/* desenha so quando nao ha mais eventos esperando:
		 * se o computador atrasar, pula quadros em vez de travar o jogo */
		if (redraw && al_is_event_queue_empty(event_queue)){
	redraw = 0;

	if (estado == INICIO){
		draw_inicio(fundo, pixel_g, pixel_p, ticks);
	} else if (estado == JOGANDO){
		draw_scenario(fundo);
		draw_hud(fonte, pontos, recorde);
		draw_enxame(&enxame);
		draw_tiro(&tiro);
		draw_canhao(&canhao, img_canhao);
	} else if (estado == DERROTA){
		if (!som_fim_tocado){
			al_play_sample(som_derrota, VOL_FIM, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
			som_fim_tocado = 1;
		}
		draw_scenario(fundo);
		draw_derrota(fonte, titulo, pontos, recorde, novo_recorde, ticks);
	} else {   /* VITORIA */
		if (!som_fim_tocado){
			al_play_sample(som_vitoria, VOL_FIM, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
			som_fim_tocado = 1;
		}
		draw_scenario(fundo);
		draw_fim(fonte, estado, pontos, recorde, novo_recorde);
	}
	al_flip_display();
	}
   }

	/* --- fim: libera recursos --- */
	al_destroy_sample(som_tiro);
	al_destroy_sample(som_explosao);
	al_destroy_sample(som_derrota);
	al_destroy_sample(som_vitoria);
	al_destroy_sample(som_inicio);
	al_destroy_sample(som_comecar);
	for (i = 0; i < 4; i++) al_destroy_sample(som_marcha[i]);

	
	if (pixel_g != titulo) al_destroy_font(pixel_g);
	if (pixel_p != fonte)  al_destroy_font(pixel_p);

	if (titulo != fonte) al_destroy_font(titulo);   /* so se for uma fonte diferente */
	al_destroy_font(fonte);
	al_destroy_bitmap(img_canhao);
	al_destroy_bitmap(fundo);
	al_destroy_timer(timer);
	al_destroy_event_queue(event_queue);
	al_destroy_display(display);

	return 0;
}