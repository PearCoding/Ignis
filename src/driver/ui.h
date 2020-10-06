#pragma once

#include "Camera.h"

void rodent_ui_init(int width, int height);
void rodent_ui_close();
void rodent_ui_settitle(const char* str);
bool rodent_ui_handleinput(uint32_t& iter, IG::Camera& cam);
void rodent_ui_update(uint32_t iter);