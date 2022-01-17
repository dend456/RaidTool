#include "icon.h"

Icon::Icon(PDIRECT3DTEXTURE9 tex, int w, int h, int tw, int th, int x, int y) : texture(tex), width(w), height(h), textureWidth(tw), textureHeight(th),
topLeft(ImVec2((float)w* x / (float)tw, (float)h* y / (float)th)), bottomRight(ImVec2((float)w* (x + 1) / (float)tw, (float)h* (y + 1) / (float)th))
{
	if (texture)
	{
		auto r = texture->AddRef();
		loaded = true;
	}
}

Icon::Icon(const Icon& o) : texture(o.texture), width(o.width), height(o.height), textureWidth(o.textureWidth), textureHeight(o.textureHeight),
topLeft(o.topLeft), bottomRight(o.bottomRight)
{
	if (texture)
	{
		auto r = texture->AddRef();
		loaded = true;
	}
}

Icon& Icon::operator=(const Icon& o)
{
	texture = o.texture;
	width = o.width;
	height = o.height;
	textureWidth = o.textureWidth;
	textureHeight = o.textureHeight;
	topLeft = o.topLeft;
	bottomRight = o.bottomRight;

	if (texture)
	{
		auto r = texture->AddRef();
		loaded = true;
	}

	return *this;
}

Icon::~Icon()
{
	if (texture)
	{
		auto r = texture->Release();
		loaded = false;
	}
}

IconInfo Icon::getIconInfo(int iconId) noexcept
{
	IconInfo i;
	i.fileId = iconId / 36 + 1;
	int n = iconId % 36;
	i.x = n / 6;
	i.y = n % 6;
	return i;
}