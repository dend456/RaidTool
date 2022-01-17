#pragma once
#include <d3d9.h>
#include "imgui.h"

struct IconInfo
{
	int x = 0;
	int y = 0;
	int fileId = 0;
};

class Icon
{
private:
	ImVec2 topLeft;
	ImVec2 bottomRight;
	PDIRECT3DTEXTURE9 texture = nullptr;
	int width = 0;
	int height = 0;
	int textureWidth = 0;
	int textureHeight = 0;
	bool loaded = false;

public:
	Icon() {}
	Icon(PDIRECT3DTEXTURE9 tex, int w, int h, int tw, int th, int x, int y);
	Icon(const Icon& o);
	Icon& operator=(const Icon& o);
	~Icon();

	ImVec2 getTopLeft() const noexcept { return topLeft; }
	ImVec2 getBottomRight() const noexcept { return bottomRight; }
	PDIRECT3DTEXTURE9 getTexture() const noexcept { return texture; }
	int getWidth() const noexcept { return width; }
	int getHeight() const noexcept { return height; }
	bool isLoaded() const noexcept { return loaded; }
	static IconInfo getIconInfo(int iconId) noexcept;
};