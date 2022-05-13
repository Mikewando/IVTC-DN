#include <GLFW/glfw3.h> // For drag-n-drop files

#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "vapoursynth/VSScript4.h"
#include "vapoursynth/VSHelper4.h"
#include "json.hpp"
#include <format>
#include <fstream>
#include <iostream>

using nlohmann::json;

ImFont* g_UbuntuMonoFont = nullptr;

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
static void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

static ImU32 ColorForAction(std::string& action) {
	static const std::map<std::string, ImU32> map = {
		{"Top Frame 0", IM_COL32(178, 34, 34, 200)},
		{"Bottom Frame 0", IM_COL32(178, 34, 34, 200)},
		{"Top Frame 1", IM_COL32(0, 0, 205, 200)},
		{"Bottom Frame 1", IM_COL32(0, 0, 205, 200)},
		{"Top Frame 2", IM_COL32(0, 128, 0, 200)},
		{"Bottom Frame 2", IM_COL32(0, 128, 0, 200)},
		{"Top Frame 3", IM_COL32(148, 0, 211, 200)},
		{"Bottom Frame 3", IM_COL32(148, 0, 211, 200)},

		{"Complete Previous Cycle", IM_COL32(255, 128, 0, 255)},

		{"Drop", IM_COL32(192, 192, 192, 200) }
	};
	return map.at(action);
}

class ExampleLayer : public Walnut::Layer
{
public:
	virtual void OnAttach() override {
		ImGuiIO io = ImGui::GetIO();
		io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	}

	void DrawField(int i) {
		int active_field = m_ActiveCycle * 10 + i;
		json &field_props = m_JsonProps["frame_props"][active_field];
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::Image(m_Fields[i]->GetDescriptorSet(), { (float)m_FieldsWidth, (float)m_FieldsHeight });
		if (ImGui::IsItemHovered()) {
			if (ImGui::IsKeyPressed(ImGuiKey_S)) {
				if (field_props["IVTC_DN_NEW_SCENE"] == 0) {
					field_props["IVTC_DN_NEW_SCENE"] = 1;
				} else {
					field_props["IVTC_DN_NEW_SCENE"] = 0;
				}
			}

			if (ImGui::IsKeyPressed(ImGuiKey_A)) {
				field_props["IVTC_DN_NOTE"] = "A";
			} else if (ImGui::IsKeyPressed(ImGuiKey_B)) {
				field_props["IVTC_DN_NOTE"] = "B";
			} else if (ImGui::IsKeyPressed(ImGuiKey_C)) {
				field_props["IVTC_DN_NOTE"] = "C";
			} else if (ImGui::IsKeyPressed(ImGuiKey_D)) {
				field_props["IVTC_DN_NOTE"] = "D";
			}

			const char* top_or_bottom = i % 2 ? "Bottom" : "Top";
			if (ImGui::IsKeyPressed(ImGuiKey_1) && i < 11) {
				std::string action = std::format("{} Frame 0", top_or_bottom);
				if (field_props["IVTC_DN_ACTION"] == action) {
					action = "Drop";
				}
				field_props["IVTC_DN_ACTION"] = action;
			} else if (ImGui::IsKeyPressed(ImGuiKey_2) && i < 11) {
				std::string action = std::format("{} Frame 1", top_or_bottom);
				if (field_props["IVTC_DN_ACTION"] == action) {
					action = "Drop";
				}
				field_props["IVTC_DN_ACTION"] = action;
			} else if (ImGui::IsKeyPressed(ImGuiKey_3) && i < 11) {
				std::string action = std::format("{} Frame 2", top_or_bottom);
				if (field_props["IVTC_DN_ACTION"] == action) {
					action = "Drop";
				}
				field_props["IVTC_DN_ACTION"] = action;
			} else if (ImGui::IsKeyPressed(ImGuiKey_4)) {
				std::string action;
				if (i < 10) {
					action = std::format("{} Frame 3", top_or_bottom);
				} else {
					action = "Complete Previous Cycle";
				}
				if (field_props["IVTC_DN_ACTION"] == action) {
					action = "Drop";
				}
				field_props["IVTC_DN_ACTION"] = action;
				std::cerr << "Field " << i << " Action: " << action;
			}
			ImGui::BeginTooltip();
			ImGui::Text("Field %d", i);
			ImGui::EndTooltip();
		}
		std::string note = field_props["IVTC_DN_NOTE"];
		std::string action = field_props["IVTC_DN_ACTION"];

		if (field_props["IVTC_DN_NEW_SCENE"] != 0) {
			ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x - 5, pos.y), ImVec2(pos.x, pos.y + 300), IM_COL32(255, 128, 0, 255));
		}
		ImVec2 text_pos(pos.x + 184, pos.y + 118);
		ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(text_pos.x - 4, text_pos.y + 5), ImVec2(text_pos.x + 40, text_pos.y + 60), ColorForAction(action));
		ImGui::GetWindowDrawList()->AddText(m_UbuntuMonoFont, 64.0f, text_pos, IM_COL32_WHITE, note.c_str());
		std::string id = std::format("field{}", i);
		ImGui::PushID(id.c_str());
		if (ImGui::BeginPopupContextItem(id.c_str())) {
			ImGui::Text("This a popup for field %d!", i);
			ImGui::EndPopup();
		}
		if (i != 10) {
			ImGui::SameLine();
		}
		ImGui::PopID();
	}

	void SaveJson() {
		std::ofstream props_file("A:\\Subs\\todo\\flcl\\01\\walnut\\props.json");
		props_file << m_JsonProps;
	}

	void ApplyCycleToScene() {
		json &props = m_JsonProps["frame_props"];
		int start_of_scene = m_ActiveCycle * 10;
		while (start_of_scene > 0 && props[start_of_scene]["IVTC_DN_NEW_SCENE"] != 1) {
			--start_of_scene;
		}
		int end_of_scene = start_of_scene + 1;
		while (end_of_scene < m_FieldsFrameCount && props[end_of_scene]["IVTC_DN_NEW_SCENE"] != 1) {
			++end_of_scene;
		}
		std::cerr << "Scene [" << start_of_scene << ", " << end_of_scene << "]" << std::endl;

		// TODO need to think about cycles a lot
		int start_of_cycle = m_ActiveCycle * 10;
		int end_of_cycle = start_of_cycle + 9;
		std::cerr << "Cycle [" << start_of_cycle << ", " << end_of_cycle << "]" << std::endl;
		std::string cycle_actions[10];
		std::string cycle_notes[10];
		int position_in_cycle = 0;
		for (int i = start_of_cycle; i <= end_of_cycle; i++) {
			cycle_actions[position_in_cycle] = props[i]["IVTC_DN_ACTION"];
			cycle_notes[position_in_cycle] = props[i]["IVTC_DN_NOTE"];
			++position_in_cycle;
		}

		// TODO need to think about cycles a lot
		position_in_cycle = start_of_scene % 10;
		for (int i = start_of_scene; i < end_of_scene; i++) {
			props[i]["IVTC_DN_ACTION"] = cycle_actions[position_in_cycle];
			props[i]["IVTC_DN_NOTE"] = cycle_notes[position_in_cycle];
			++position_in_cycle %= 10;
		}
	}

	virtual void OnUIRender() override {
		static int error = 0;
		static char error_message[1024];

		static int last_cycle = -1;
		m_NeedNewFields |= last_cycle != m_ActiveCycle;
		last_cycle = m_ActiveCycle;

		int max_cycle = m_FieldsFrameCount / 10;

		if (m_ActiveCycle < max_cycle && (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_J))) {
			m_ActiveCycle++;
		}
		if (m_ActiveCycle > 0 && (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_K))) {
			m_ActiveCycle--;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_R)) {
			set_active_frames(m_ActiveFile);
		}

		if (ImGui::IsKeyPressed(ImGuiKey_T)) {
			ApplyCycleToScene();
		}

		ImGui::Begin("Fields");

		for (int i = 0; i < 11; i++) {
			if (m_NeedNewFields && !error) {
				const VSFrame* frame = m_VSAPI->getFrame(m_ActiveCycle * 10 + i, m_FieldsNode, error_message, sizeof(error_message));
				if (!frame) {
					fprintf(stderr, error_message);
					error = 1;
				}
				const uint8_t* frame_ptr = m_VSAPI->getReadPtr(frame, 0);
				m_Fields[i]->SetData(frame_ptr);
				m_VSAPI->freeFrame(frame);
			}
		}

		// Top Fields
		for (int i = 0; i < 11; i += 2) {
			DrawField(i);
		}

		// Bottom Fields
		for (int i = 1; i < 10; i += 2) {
			DrawField(i);
		}

		ImGui::End();

		ImGui::Begin("Output");

		for (int i = 0; i < 4; i++) {
			if (m_NeedNewFields && !error) {
				const VSFrame* frame = m_VSAPI->getFrame(m_ActiveCycle * 4 + i, m_FramesNode, error_message, sizeof(error_message));
				if (!frame) {
					fprintf(stderr, error_message);
					error = 1;
				}
				const uint8_t* frame_ptr = m_VSAPI->getReadPtr(frame, 0);
				m_Frames[i]->SetData(frame_ptr);
				m_VSAPI->freeFrame(frame);
			}
		}
		for (int i = 0; i < 4; i++) {
			ImGui::Image(m_Frames[i]->GetDescriptorSet(), { (float)m_FramesWidth, (float)m_FramesHeight });
			if (i < 4) {
				ImGui::SameLine();
			}
		}

		ImGui::End();

		ImGui::Begin("Controls");
		ImGui::SliderInt("Active Cycle", &m_ActiveCycle, 0, max_cycle);
		ImGui::SameLine(); HelpMarker("CTRL+click to input value.");

		ImGui::End();
		ImGui::ShowDemoWindow();

		if (ImGui::IsKeyPressed(ImGuiKey_W)) {
			SaveJson();
		}
	}

	ExampleLayer() {
		m_VSSAPI = getVSScriptAPI(VSSCRIPT_API_VERSION);
		if (!m_VSSAPI) {
			// VapourSynth probably isn't properly installed at all
			fprintf(stderr, "Failed to initialize VSScript library\n");
		}
		assert(m_VSSAPI);

		// Get a pointer to the normal api struct, exists so you don't have to link with the VapourSynth core library
		// Failure only happens on very rare API version mismatches and usually doesn't need to be checked
		m_VSAPI = m_VSSAPI->getVSAPI(VAPOURSYNTH_API_VERSION);
		assert(m_VSAPI);

		std::ifstream props_file("A:\\Subs\\todo\\flcl\\01\\walnut\\props.json");
		props_file >> m_JsonProps;
		fprintf(stderr, "%zd Frame Props loaded\n", m_JsonProps["frame_props"].size());
		//set_active_fields("A:\\Subs\\todo\\flcl\\01\\walnut\\separate_fields.vpy");
		//set_active_fields("A:\\Subs\\todo\\garzey\\01\\line doubling\\separate fields.vpy");
		set_active_fields("A:\\Subs\\todo\\otaku no video\\01\\walnut\\fields.vpy");
		set_active_frames("A:\\Subs\\todo\\flcl\\01\\walnut\\output frames.vpy");
	}

	void set_active_fields(const char* file) {
		if (m_FieldsScriptEnvironment != nullptr) {
			m_VSAPI->freeNode(m_FieldsNode);
			m_VSSAPI->freeScript(m_FieldsScriptEnvironment);
		}
		m_FieldsScriptEnvironment = m_VSSAPI->createScript(nullptr);

		m_VSSAPI->evalSetWorkingDir(m_FieldsScriptEnvironment, 1);
		int error = m_VSSAPI->evaluateFile(m_FieldsScriptEnvironment, file);
		if (error != 0) {
			fprintf(stderr, "Error loading file: %s\n", m_VSSAPI->getError(m_FieldsScriptEnvironment));
		}

		m_FieldsNode = m_VSSAPI->getOutputNode(m_FieldsScriptEnvironment, 0);
		const VSVideoInfo* vi = m_VSAPI->getVideoInfo(m_FieldsNode);
		if (vi->format.colorFamily == cfYUV) {
			// Convert to RGB & pack
			VSCore* core = m_VSSAPI->getCore(m_FieldsScriptEnvironment);
			ConvertToRGB(core, m_FieldsNode);
			ShufflePlanes(core, m_FieldsNode);
			Pack(core, m_FieldsNode);
		} else if (vi->format.colorFamily == cfRGB) {
			VSCore* core = m_VSSAPI->getCore(m_FieldsScriptEnvironment);
			ShufflePlanes(core, m_FieldsNode);
			Pack(core, m_FieldsNode);
		} else {
			// Hope for the best?
		}
		fprintf(stderr, "Video Width: %d x %d\n", vi->width, vi->height);
		fprintf(stderr, "Video Format: %d, %d, %d\n", vi->format.colorFamily, vi->format.bitsPerSample, vi->format.numPlanes);
		m_FieldsWidth = vi->width;
		m_FieldsHeight = vi->height;
		m_FieldsFrameCount = vi->numFrames;

		for (int i = 0; i < 11; i++) {
			m_Fields[i] = std::make_shared<Walnut::Image>(
				m_FieldsWidth,
				m_FieldsHeight,
				Walnut::ImageFormat::RGBA,
				nullptr);
		}
	}

	void set_active_frames(const char* file) {
		if (m_FramesScriptEnvironment != nullptr) {
			m_VSAPI->freeNode(m_FramesNode);
			m_VSSAPI->freeScript(m_FramesScriptEnvironment);
		}
		m_FramesScriptEnvironment = m_VSSAPI->createScript(nullptr);

		int error = m_VSSAPI->evaluateFile(m_FramesScriptEnvironment, file);
		if (error != 0) {
			fprintf(stderr, "Error loading file: %s\n", m_VSSAPI->getError(m_FramesScriptEnvironment));
		}

		m_FramesNode = m_VSSAPI->getOutputNode(m_FramesScriptEnvironment, 0);
		const VSVideoInfo* vi = m_VSAPI->getVideoInfo(m_FramesNode);
		fprintf(stderr, "Video Width: %d x %d\n", vi->width, vi->height);
		fprintf(stderr, "Video Format: %d, %d, %d\n", vi->format.colorFamily, vi->format.bitsPerSample, vi->format.numPlanes);
		m_FramesWidth = vi->width;
		m_FramesHeight = vi->height;
		m_FramesFrameCount = vi->numFrames;

		for (int i = 0; i < 4; i++) {
			m_Frames[i] = std::make_shared<Walnut::Image>(
				m_FramesWidth,
				m_FramesHeight,
				Walnut::ImageFormat::RGBA,
				nullptr);
		}
		m_ActiveFile = file;
		m_NeedNewFields = true;
	}


private:
	ImFont* m_UbuntuMonoFont = nullptr;

	const VSAPI* m_VSAPI = nullptr;
	const VSSCRIPTAPI* m_VSSAPI = nullptr;
	const char* m_ActiveFile; // TODO
	json m_JsonProps;

	int m_ActiveCycle = 1316;
	bool m_NeedNewFields = false;

	// Fields
	VSScript* m_FieldsScriptEnvironment = nullptr;
	VSNode* m_FieldsNode = nullptr;
	int m_FieldsWidth = 0;
	int m_FieldsHeight = 0;
	int m_FieldsFrameCount = 0;
	std::shared_ptr<Walnut::Image> m_Fields[11];

	// Frames
	VSScript* m_FramesScriptEnvironment = nullptr;
	VSNode* m_FramesNode = nullptr;
	int m_FramesWidth = 0;
	int m_FramesHeight = 0;
	int m_FramesFrameCount = 0;
	std::shared_ptr<Walnut::Image> m_Frames[4];

	void ConvertToRGB(VSCore* core, VSNode* &node) {
		VSMap* argument_map = m_VSAPI->createMap();
		VSPlugin* resize_plugin = m_VSAPI->getPluginByID("com.vapoursynth.resize", core);
		m_VSAPI->mapConsumeNode(argument_map, "clip", node, maReplace);
		m_VSAPI->mapSetInt(argument_map, "format", pfRGB24, maReplace);
		m_VSAPI->mapSetInt(argument_map, "matrix_in", 1, maReplace);
		VSMap* result_map = m_VSAPI->invoke(resize_plugin, "Spline36", argument_map);

		const char* result_error = m_VSAPI->mapGetError(result_map);
		if (result_error) {
			fprintf(stderr, "%s\n", result_error);
		}

		node = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
	}

	void ShufflePlanes(VSCore* core, VSNode* &node) {
		VSMap* argument_map = m_VSAPI->createMap();
		VSPlugin* std_plugin = m_VSAPI->getPluginByID("com.vapoursynth.std", core);
		m_VSAPI->mapConsumeNode(argument_map, "clips", node, maReplace);
		const int64_t planes[] = {2, 1, 0};
		m_VSAPI->mapSetIntArray(argument_map, "planes", planes, 3);
		m_VSAPI->mapSetInt(argument_map, "colorfamily", cfRGB, maReplace);
		VSMap* result_map = m_VSAPI->invoke(std_plugin, "ShufflePlanes", argument_map);

		const char* result_error = m_VSAPI->mapGetError(result_map);
		if (result_error) {
			fprintf(stderr, "%s\n", result_error);
		}

		node = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
	}

	void Pack(VSCore* core, VSNode* &node) {
		VSMap* argument_map = m_VSAPI->createMap();
		VSPlugin* libp2p_plugin = m_VSAPI->getPluginByID("com.djatom.libp2p", core);
		m_VSAPI->mapConsumeNode(argument_map, "clip", node, maReplace);
		VSMap* result_map = m_VSAPI->invoke(libp2p_plugin, "Pack", argument_map);

		const char* result_error = m_VSAPI->mapGetError(result_map);
		if (result_error) {
			fprintf(stderr, "%s\n", result_error);
		}

		node = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
	}
};

std::shared_ptr<ExampleLayer> g_Layer = nullptr;

void glfw_drop_callback(GLFWwindow* window, int path_count, const char* paths[]) {
	fprintf(stderr, "Path count: %d, First Path: %s\n", path_count, paths[0]);
	g_Layer->set_active_frames(paths[0]);
	return;
}

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "IVTC DN";

	Walnut::Application* app = new Walnut::Application(spec);
	g_UbuntuMonoFont = app->m_UbuntuMonoFont;
	g_Layer = std::make_shared<ExampleLayer>();
	app->PushLayer(g_Layer);
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	glfwSetDropCallback(app->GetWindowHandle(), glfw_drop_callback);
	return app;
}