/*
 * Copyright 2018 Ales Mlakar. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

 /*
  * Reference(s):
  * - Sparse Virtual Textures by Sean Barrett
  *   http://web.archive.org/web/20190103162611/http://silverspaceship.com/src/svt/
  * - Based on Virtual Texture Demo by Brad Blanchard
  *   http://web.archive.org/web/20190103162638/http://linedef.com/virtual-texture-demo.html
  * - Mars texture
  *   http://web.archive.org/web/20190103162730/http://www.celestiamotherlode.net/catalog/mars.php
  */

#include <bx/file.h>
#include "common.h"
#include "bgfx_utils.h"
#include "imgui/imgui.h"
#include "camera.h"
#include "vt.h"

namespace
{

struct PosTexcoordVertex
{
	float    m_x;
	float    m_y;
	float    m_z;
	float    m_u;
	float    m_v;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	};

	static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosTexcoordVertex::ms_layout;

static const float s_planeScale = 256.0f;

static PosTexcoordVertex s_vplaneVertices[] =
{
	{ -s_planeScale, 0.0f,  s_planeScale, 1.0f, 1.0f },
	{  s_planeScale, 0.0f,  s_planeScale, 1.0f, 0.0f },
	{ -s_planeScale, 0.0f, -s_planeScale, 0.0f, 1.0f },
	{  s_planeScale, 0.0f, -s_planeScale, 0.0f, 0.0f },
};

static const uint16_t s_planeIndices[] =
{
	0, 1, 2,
	1, 3, 2,
};

class ExampleSVT : public entry::AppI
{
public:
	ExampleSVT(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
	{
	}

	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
	{
		Args args(_argc, _argv);

		m_width = _width;
		m_height = _height;
		m_debug = BGFX_DEBUG_TEXT;
		m_reset = BGFX_RESET_VSYNC;

		bgfx::Init init;

		init.type = args.m_type;
		init.vendorId = args.m_pciId;
		init.resolution.width = m_width;
		init.resolution.height = m_height;
		init.resolution.reset = m_reset;
		bgfx::init(init);

		// Enable m_debug text.
		bgfx::setDebug(m_debug);

		// Set views clear state (first pass to 0, second pass to some background color)
		for (uint16_t i = 0; i < 2; ++i)
		{
			bgfx::setViewClear(i
				, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
				, i == 0 ? 0 : 0x101050ff
				, 1.0f
				, 0
			);
		}

		// Create vertex stream declaration.
		PosTexcoordVertex::init();

		{
			const float size = 1024.0f;
			const int tesselation = 512;
			const float ooTesselation = 1.0f / (float)tesselation;

			const auto vertexCount = (tesselation + 1) * (tesselation + 1);
			const auto vertexSize = vertexCount * sizeof(PosTexcoordVertex);
			PosTexcoordVertex* vertices = (PosTexcoordVertex*)BX_ALLOC(&m_vtAllocator, vertexSize);
			const auto indexCount = (tesselation * tesselation) * 6;
			const auto indexSize = indexCount * sizeof(uint32_t); 
			uint32_t* indices = (uint32_t*)BX_ALLOC(&m_vtAllocator, indexSize);

			auto indexIndex = 0;
			auto vertexIndex = 0;
			auto currentVertex = vertices;
			auto currentIndex = indices;
			for(int y=0;y<=tesselation;++y)
			{
				const float v = (float)y * ooTesselation;
				const float yPosition = v * size;
				for(int x=0;x<=tesselation;++x, ++currentVertex, ++vertexIndex)
				{
					const float u = (float)x * ooTesselation;
					const float xPosition = u * size;
					currentVertex->m_x = xPosition;
					currentVertex->m_y = 0.0f;
					currentVertex->m_z = yPosition;
					currentVertex->m_u = 1.0 - u;
					currentVertex->m_v = v;

					if(x<tesselation && y<tesselation)
					{
						currentIndex[2] = vertexIndex;
						currentIndex[1] = vertexIndex + 1;
						currentIndex[0] = vertexIndex + tesselation + 1;

						currentIndex[3] = vertexIndex + tesselation + 1;
						currentIndex[4] = vertexIndex + 1;
						currentIndex[5] = vertexIndex + tesselation + 2;

						currentIndex += 6;
						indexIndex += 6;

					}
				}
			}

			BX_ASSERT(vertexCount == vertexIndex, "Vertex count fail!");
			BX_ASSERT(indexCount == indexIndex, "Index count fail!");
			// Create static vertex buffer.
			m_vbh = bgfx::createVertexBuffer(bgfx::makeRef(vertices, vertexSize), PosTexcoordVertex::ms_layout);
			m_ibh = bgfx::createIndexBuffer(bgfx::makeRef(indices, indexSize), BGFX_BUFFER_INDEX32);
		}


		/*
		// Create static vertex buffer.
		m_vbh = bgfx::createVertexBuffer(
			  bgfx::makeRef(s_vplaneVertices, sizeof(s_vplaneVertices))
			, PosTexcoordVertex::ms_layout
		);

		m_ibh = bgfx::createIndexBuffer(
			  bgfx::makeRef(s_planeIndices, sizeof(s_planeIndices))
		);
		*/

		// Create program from shaders.
		m_vt_unlit = loadProgram("vs_vt_generic", "fs_vt_unlit");
		m_vt_mip = loadProgram("vs_vt_generic", "fs_vt_mip");

		// Imgui.
		imguiCreate();

		m_timeOffset = bx::getHPCounter();

		// Get renderer capabilities info.
		m_caps = bgfx::getCaps();

		m_scrollArea = 0;

		// Create and setup camera
		cameraCreate();

		cameraSetPosition({ 0.0f, 5.0f, 0.0f });
		cameraSetVerticalAngle(0.0f);

		// Set VirtualTexture system allocator
		vt::VirtualTexture::setAllocator(&m_vtAllocator);

		// Create Virtual texture info
		m_vti = new vt::VirtualTextureInfo();
		m_vti->m_virtualTextureSize = 8192; // The actual size will be read from the tile data file
		m_vti->m_tileSize = 256;
		m_vti->m_borderSize = 1;		

		// Generate tile data file (if not yet created)
		{
			vt::TileGenerator tileGenerator(m_vti);
			//tileGenerator.generate("d:/FalconBMS/Falcon BMS 4.37 (Internal)/Data/TerrData/Korea/NewTerrain/Photoreal/4K/", "base4k", 4096, 16);
			tileGenerator.generate("d:/FalconBMS/Falcon BMS 4.37 (Internal)/Data/TerrData/Korea/NewTerrain/Photoreal/16K/", "base16k", 16 * 1024, 16);
		}

		// Load heightmap
		{

			bx::Error err;
			bx::FileReader fileReader;

			if (!bx::open(&fileReader, "d:/FalconBMS/Falcon BMS 4.37 (Internal)/Data/TerrData/Korea/NewTerrain/HeightMaps/HeightMap.raw", &err) )
			{
				return;
			}

			const auto size = bx::getSize(&fileReader);
			auto data = (int16_t*)BX_ALLOC(&m_vtAllocator, size);
			bx::read(&fileReader, data, size, &err);
			bx::close(&fileReader);


			const int sourceSize = 32 * 1024;
			const int targetSize = 16 * 1024;
			const int targetData16 = targetSize * targetSize * sizeof(float);
			auto data16 = (float*)BX_ALLOC(&m_vtAllocator, targetData16);

			for(int y=0;y<sourceSize;y+=2)
			{
				for(int x=0;x<sourceSize;x+=2)
				{
					data16[(y >> 1) * targetSize + (x >> 1)] = (float)data[y * sourceSize + x] * 0.0001f;
				}
			}

			m_heightMap = bgfx::createTexture2D(16 * 1024, 16 * 1024, false, 1, bgfx::TextureFormat::R32F, 0, bgfx::copy(data16, targetData16));

			m_tex2 = bgfx::createUniform("s_tex2", bgfx::UniformType::Sampler);
		}

		// Load tile data file
		//auto tileDataFile = new vt::TileDataFile("temp/base4k.vt", m_vti);
		auto tileDataFile = new vt::TileDataFile("temp/base16k.vt", m_vti);
		tileDataFile->readInfo();

		// Create virtual texture and feedback buffer
		m_vt = new vt::VirtualTexture(tileDataFile, m_vti, 4096, 1, 6);
		m_feedbackBuffer = new vt::FeedbackBuffer(m_vti, 64, 64);

		//bgfx::TextureInfo info;
		//bgfx::TextureFormat::ATC

	}

	virtual int shutdown() override
	{
		// Cleanup.
		bgfx::frame();

		cameraDestroy();
		imguiDestroy();

		bgfx::destroy(m_ibh);
		bgfx::destroy(m_vbh);

		bgfx::destroy(m_vt_unlit);
		bgfx::destroy(m_vt_mip);

		delete m_vti;
		delete m_vt;
		delete m_feedbackBuffer;

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() override
	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState))
		{
			imguiBeginFrame(
				  m_mouseState.m_mx
				, m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				, m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
			);

			showExampleDialog(this);

			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency());
			const float deltaTime = float(frameTime / freq);

			float time = (float)((now - m_timeOffset) / freq);

			if ((BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK) != (bgfx::getCaps()->supported & (BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK)))
			{
				// When texture read-back or blit is not supported by GPU blink!
				bool blink = uint32_t(time*3.0f) & 1;
				bgfx::dbgTextPrintf(0, 0, blink ? 0x4f : 0x04, " Texture read-back and/or blit not supported by GPU. ");

				// Set view 0 default viewport.
				bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));

				// This dummy draw call is here to make sure that view 0 is cleared
				// if no other draw calls are submitted to view 0.
				bgfx::touch(0);
			}
			else
			{
				ImGui::SetNextWindowPos(
					  ImVec2(m_width - m_width / 5.0f - 10.0f, 10.0f)
					, ImGuiCond_FirstUseEver
				);
				ImGui::SetNextWindowSize(
					  ImVec2(m_width / 5.0f, m_height - 10.0f)
					, ImGuiCond_FirstUseEver
				);
				ImGui::Begin("Settings"
					, NULL
					, 0
				);

				//ImGui::SliderFloat("intensity", &m_intensity, 0.0f, 3.0f);
				auto showBorders = m_vt->isShowBoardersEnabled();
				if (ImGui::Checkbox("Show borders", &showBorders))
				{
					m_vt->enableShowBoarders(showBorders);
				}
				auto colorMipLevels = m_vt->isColorMipLevelsEnabled();
				if (ImGui::Checkbox("Color mip levels", &colorMipLevels))
				{
					m_vt->enableColorMipLevels(colorMipLevels);
				}
				auto uploadsperframe = m_vt->getUploadsPerFrame();
				if (ImGui::InputInt("Updates per frame", &uploadsperframe, 1, 2))
				{
					uploadsperframe = bx::clamp(uploadsperframe, 1, 100);
					m_vt->setUploadsPerFrame(uploadsperframe);
				}

				ImGui::ImageButton(m_vt->getAtlastTexture(), ImVec2(m_width / 5.0f - 16.0f, m_width / 5.0f - 16.0f));
				ImGui::ImageButton(bgfx::getTexture(m_feedbackBuffer->getFrameBuffer()), ImVec2(m_width / 5.0f - 16.0f, m_width / 5.0f - 16.0f));

				ImGui::End();

				// Update camera.
				cameraUpdate(deltaTime, m_mouseState, ImGui::MouseOverArea() );

				float view[16];
				cameraGetViewMtx(view);

				float proj[16];
				bx::mtxProj(proj, 90.0f, float(m_width) / float(m_height), 0.1f, 100000.0f, m_caps->homogeneousDepth);

				// Setup views
				for (uint16_t i = 0; i < 2; ++i)
				{
					uint16_t viewWidth = 0;
					uint16_t viewHeight = 0;
					// Setup pass, first pass is into mip-map feedback buffer, second pass is on screen
					if (i == 0)
					{
						bgfx::setViewFrameBuffer(i, m_feedbackBuffer->getFrameBuffer());
						viewWidth = uint16_t(m_feedbackBuffer->getWidth());
						viewHeight = uint16_t(m_feedbackBuffer->getHeight());
					}
					else
					{
						bgfx::FrameBufferHandle invalid = BGFX_INVALID_HANDLE;
						bgfx::setViewFrameBuffer(i, invalid);
						viewWidth = uint16_t(m_width);
						viewHeight = uint16_t(m_height);
					}

					bgfx::setViewRect(i, 0, 0, viewWidth, viewHeight);
					bgfx::setViewTransform(i, view, proj);

					float mtx[16];
					bx::mtxIdentity(mtx);

					// Set identity transform for draw call.
					bgfx::setTransform(mtx);

					// Set vertex and index buffer.
					bgfx::setVertexBuffer(0, m_vbh);
					bgfx::setIndexBuffer(m_ibh);

					// Set render states.
					bgfx::setState(0
						| BGFX_STATE_WRITE_RGB
						| BGFX_STATE_WRITE_A
						| BGFX_STATE_WRITE_Z
						| BGFX_STATE_DEPTH_TEST_LESS
					);

					// Set virtual texture uniforms
					m_vt->setUniforms();
					bgfx::setTexture(4, m_tex2, m_heightMap);

					// Submit primitive for rendering to first pass (to feedback buffer, where mip levels and tile x/y will be rendered
					if (i == 0)
					{
						bgfx::submit(i, m_vt_mip);
						// Download previous frame feedback info
						m_feedbackBuffer->download();
						// Update and upload new requests
						m_vt->update(m_feedbackBuffer->getRequests(), 4);
						// Clear feedback
						m_feedbackBuffer->clear();
						// Copy new frame feedback buffer
						m_feedbackBuffer->copy(3);
					}
					else
					{
						// Submit primitive for rendering to second pass (to back buffer, where virtual texture page table and atlas will be used)
						bgfx::submit(i, m_vt_unlit);
					}
				}
			}

			imguiEndFrame();

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();

			return true;
		}

		return false;
	}

	bgfx::TextureHandle m_heightMap;

	bgfx::VertexBufferHandle m_vbh;
	bgfx::IndexBufferHandle m_ibh;

	bgfx::ProgramHandle m_vt_unlit;
	bgfx::ProgramHandle m_vt_mip;

	bgfx::UniformHandle m_tex2;

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;

	int32_t m_scrollArea;

	entry::MouseState m_mouseState;

	const bgfx::Caps* m_caps;
	int64_t m_timeOffset;

	bx::DefaultAllocator m_vtAllocator;
	vt::VirtualTextureInfo* m_vti;
	vt::VirtualTexture* m_vt;
	vt::FeedbackBuffer* m_feedbackBuffer;
	
};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	  ExampleSVT
	, "40-svt"
	, "Sparse Virtual Textures."
	, "https://bkaradzic.github.io/bgfx/examples.html#svt"
	);
