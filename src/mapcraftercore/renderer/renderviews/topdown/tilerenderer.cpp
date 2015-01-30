/*
 * Copyright 2012-2015 Moritz Hilscher
 *
 * This file is part of Mapcrafter.
 *
 * Mapcrafter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mapcrafter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mapcrafter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tilerenderer.h"

#include "../../biomes.h"
#include "../../blockimages.h"
#include "../../image.h"
#include "../../rendermode.h"
#include "../../tileset.h"
#include "../../../mc/pos.h"
#include "../../../mc/worldcache.h"

#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace mapcrafter {
namespace renderer {

TopdownTileRenderer::TopdownTileRenderer(BlockImages* images, int tile_width,
		mc::WorldCache* world, RenderModes& render_modes)
	: TileRenderer(images, tile_width, world, render_modes) {
}

TopdownTileRenderer::~TopdownTileRenderer() {
}

void TopdownTileRenderer::renderChunk(const mc::Chunk& chunk, RGBAImage& tile, int dx, int dy) {
	int texture_size = images->getTextureSize();

	for (int x = 0; x < 16; x++)
		for (int z = 0; z < 16; z++) {
			std::deque<RGBAImage> blocks;

			// TODO make this water thing a bit nicer
			bool in_water = false;

			mc::LocalBlockPos localpos(x, z, 0);
			//int height = chunk.getHeightAt(localpos);
			//localpos.y = height;
			localpos.y = -1;
			if (localpos.y >= mc::CHUNK_HEIGHT*16 || localpos.y < 0)
				localpos.y = mc::CHUNK_HEIGHT*16 - 1;

			uint16_t id = chunk.getBlockID(localpos);
			while (id == 0 && localpos.y > 0) {
				localpos.y--;
				id = chunk.getBlockID(localpos);
			}
			if (localpos.y < 0)
				continue;

			while (localpos.y >= 0) {
				mc::BlockPos globalpos = localpos.toGlobalPos(chunk.getPos());

				id = chunk.getBlockID(localpos);
				if (id == 0) {
					in_water = false;
					localpos.y--;
					continue;
				}
				uint16_t data = chunk.getBlockData(localpos);

				bool is_water = (id == 8 || id == 9) && data == 0;

				bool hidden = false;
				for (size_t i = 0; i < render_modes.size(); i++) {
					if (render_modes[i]->isHidden(globalpos, id, data)) {
						hidden = true;
						break;
					}
				}
				if (hidden) {
					localpos.y--;
					continue;
				}

				if (is_water) {
					if (is_water == in_water) {
						localpos.y--;
						continue;
					}
					in_water = is_water;
				}

				RGBAImage block = images->getBlock(id, data);
				if (Biome::isBiomeBlock(id, data)) {
					block = images->getBiomeDependBlock(id, data, getBiomeOfBlock(globalpos, &chunk));
				}

				for (size_t i = 0; i < render_modes.size(); i++)
					render_modes[i]->draw(block, globalpos, id, data);

				blocks.push_back(block);
				if (!images->isBlockTransparent(id, data)) {
					break;
				}
				localpos.y--;
			}

			while (blocks.size() > 0) {
				RGBAImage block = blocks.back();
				tile.alphaBlit(block, dx + x*texture_size, dy + z*texture_size);
				blocks.pop_back();
			}
		}
}

void TopdownTileRenderer::renderTile(const TilePos& tile_pos, RGBAImage& tile) {
	int texture_size = images->getTextureSize();
	tile.setSize(getTileSize(), getTileSize());

	// call start method of the rendermodes
	for (size_t i = 0; i < render_modes.size(); i++)
		render_modes[i]->start();

	for (int x = 0; x < tile_width; x++)
		for (int z = 0; z < tile_width; z++) {
			mc::ChunkPos chunkpos(tile_pos.getX() * tile_width + x, tile_pos.getY() * tile_width + z);
			current_chunk = world->getChunk(chunkpos);
			if (current_chunk != nullptr)
				renderChunk(*current_chunk, tile, texture_size*16*x, texture_size*16*z);
		}

	// call the end method of the rendermodes
	for (size_t i = 0; i < render_modes.size(); i++)
		render_modes[i]->end();
}

int TopdownTileRenderer::getTileSize() const {
	return images->getBlockSize() * 16 * tile_width;
}

}
}