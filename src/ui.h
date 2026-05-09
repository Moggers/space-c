#ifndef GAME_UI
#define GAME_UI

#include "./contracts.h"
#include "ai.h"
#include "entities.h"
#include "vlk_nk.h"
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

int32_t ENTITY_CONTEXT_MENU   = -1;
int32_t CONTEXT_MENU_ENTITYID = -1;
struct nk_context *UI_CTX;
float CTX_MENU_X, CTX_MENU_Y;
float CTRCT_X, CTRCT_Y;
uint32_t UI_CTRCTS_ENTITYID = -1;

void ui_close_ctx() { CONTEXT_MENU_ENTITYID = -1; }

void ui_draw(SDL_Window *win) {

  int winwidth, winheight;
  SDL_GetWindowSize(win, &winwidth, &winheight);
  float mousex, mousey;
  SDL_GetMouseState(&mousex, &mousey);

  if (CONTEXT_MENU_ENTITYID != -1) {
    nk_style_push_style_item(UI_CTX, &UI_CTX->style.window.fixed_background,
                             nk_style_item_color(nk_rgba(30, 30, 30, 30)));
    nk_style_push_vec2(UI_CTX, &UI_CTX->style.window.padding, nk_vec2(0, 0));
    nk_style_push_vec2(UI_CTX, &UI_CTX->style.window.spacing, nk_vec2(0, 0));
    nk_style_push_float(UI_CTX, &UI_CTX->style.window.rounding, 0);
    nk_style_push_float(UI_CTX, &UI_CTX->style.window.border, 0);
    nk_style_push_float(UI_CTX, &UI_CTX->style.button.rounding, 0);
    nk_style_push_vec2(UI_CTX, &UI_CTX->style.button.padding, nk_vec2(0, 0));
    nk_style_push_float(UI_CTX, &UI_CTX->style.button.border, 0);
    if (nk_begin(UI_CTX, "ctxmenu", nk_rect(CTX_MENU_X, CTX_MENU_Y, 70, 100),
                 NK_WINDOW_NO_SCROLLBAR)) {
      nk_layout_row_static(UI_CTX, 15, 70, 1);
      if (nk_button_label(UI_CTX, "contracts")) {
        UI_CTRCTS_ENTITYID    = CONTEXT_MENU_ENTITYID;
        CTRCT_X               = fmin(winwidth - 600, mousex);
        CTRCT_Y               = CTX_MENU_Y;
        CONTEXT_MENU_ENTITYID = -1;
      }
    }
    nk_end(UI_CTX);
    nk_style_pop_vec2(UI_CTX);
    nk_style_pop_float(UI_CTX);
    nk_style_pop_float(UI_CTX);
    nk_style_pop_float(UI_CTX);
    nk_style_pop_float(UI_CTX);
    nk_style_pop_vec2(UI_CTX);
    nk_style_pop_vec2(UI_CTX);
    nk_style_pop_style_item(UI_CTX);
  }
  if (UI_CTRCTS_ENTITYID != -1) {
    if (nk_begin(UI_CTX, "Contracts", nk_rect(CTRCT_X, CTRCT_Y, 600, 600),
                 NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE |
                     NK_WINDOW_CLOSABLE)) {
      nk_layout_row_dynamic(UI_CTX, 40, 1);
      char header[64];
      sprintf(header, "Contracts for %s", ENTITY_NAME[UI_CTRCTS_ENTITYID]);
      nk_label(UI_CTX, header, NK_TEXT_ALIGN_LEFT);
      char amount[32];
      for (uint32_t i = 0; i < 32; i++) {
        uint32_t contract_id = ENTITY_CONTRACTS[UI_CTRCTS_ENTITYID][i];
        if (contract_id == 0) {
          continue;
        }
        nk_layout_row_dynamic(UI_CTX, 20, 3);
        nk_label(UI_CTX, "Item", NK_TEXT_LEFT);
        nk_label(UI_CTX, "Amount", NK_TEXT_LEFT);
        nk_label(UI_CTX, "Claimant", NK_TEXT_LEFT);
        nk_label(UI_CTX, CONTRACT_NAME[contract_id], NK_TEXT_LEFT);
        sprintf(amount, "%d", CONTRACT_AMOUNT[contract_id]);
        nk_label(UI_CTX, amount, NK_TEXT_LEFT);
        if (CONTRACT_CLAIMING_ENTITYID[contract_id] == 0) {
          nk_label(UI_CTX, "Unclaimed", NK_TEXT_LEFT);
        } else {
          nk_label(UI_CTX, ENTITY_NAME[CONTRACT_CLAIMING_ENTITYID[contract_id]],
                   NK_TEXT_LEFT);
        }
      }
    }
    nk_end(UI_CTX);
  }
  if (nk_window_is_hidden(UI_CTX, "Contracts")) {
    UI_CTRCTS_ENTITYID = -1;
  }
}

void ui_open_context_menu_for(uint32_t entity) {
  CONTEXT_MENU_ENTITYID = entity;
  SDL_GetMouseState(&CTX_MENU_X, &CTX_MENU_Y);
}
#endif
