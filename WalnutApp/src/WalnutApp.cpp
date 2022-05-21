#include "gzip/compress.hpp"
#include "gzip/decompress.hpp"
#include "ImGuiFileDialog.h"
#include "json.hpp"
#include "vapoursynth/VSScript4.h"
#include "vapoursynth/VSHelper4.h"
#include "Walnut/Image.h"
#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include <format>
#include <fstream>
#include <GLFW/glfw3.h> // For drag-n-drop files
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

static ImU32 ColorForAction(const int_fast8_t action) {
	static const std::map<int_fast8_t, ImU32> map = {
		{0, IM_COL32(178, 34, 34, 200)},    // Top Frame 0
		{1, IM_COL32(178, 34, 34, 200)},    // Bottom Frame 0
		{2, IM_COL32(0, 0, 205, 200)},      // Top Frame 1
		{3, IM_COL32(0, 0, 205, 200)},      // Bottom Frame 1
		{4, IM_COL32(0, 128, 0, 200)},      // Top Frame 2
		{5, IM_COL32(0, 128, 0, 200)},      // Bottom Frame 2
		{6, IM_COL32(148, 0, 211, 200)},    // Top Frame 3
		{7, IM_COL32(148, 0, 211, 200)},    // Bottom Frame 3

		{8, IM_COL32(192, 192, 192, 200) }, // Drop

		{9, IM_COL32(255, 128, 0, 255)},    // Complete Previous Cycle
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

	virtual void OnUIRender() override {
		static int error = 0;
		static char error_message[1024];

		if (ImGuiFileDialog::Instance()->Display("NewProjectDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
			if (ImGuiFileDialog::Instance()->IsOk()) {
				std::string scriptPathName = ImGuiFileDialog::Instance()->GetFilePathName();
				StartNewProject(scriptPathName.c_str());
			}
			ImGuiFileDialog::Instance()->Close();
		}

		if (ImGuiFileDialog::Instance()->Display("OpenProjectDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
			if (ImGuiFileDialog::Instance()->IsOk()) {
				std::string projectPathName = ImGuiFileDialog::Instance()->GetFilePathName();
				OpenProject(projectPathName.c_str());
			}
			ImGuiFileDialog::Instance()->Close();
		}

		if (ImGuiFileDialog::Instance()->Display("SaveProjectAsDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
			if (ImGuiFileDialog::Instance()->IsOk()) {
				std::string projectPathName = ImGuiFileDialog::Instance()->GetFilePathName();
				m_ProjectFile = projectPathName;
				SaveJson();
			}
			ImGuiFileDialog::Instance()->Close();
		}

		if (ImGuiFileDialog::Instance()->IsOpened()) {
			return;
		}

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
			LoadFrames();
		}

		if (ImGui::IsKeyPressed(ImGuiKey_T)) {
			ApplyCycleToScene();
		}

		ImGui::Begin("Fields");

		for (int i = 0; i < 11; i++) {
			if (m_Fields[i] == nullptr) {
				continue;
			}

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

		ImVec2 availableSpace = ImGui::GetContentRegionAvail();
		float fieldDisplayWidth = availableSpace.x / 6.15f; // TODO maybe tables make this more precise?
		float fieldDisplayHeight = m_FramesWidth ? fieldDisplayWidth * ((float)m_FramesHeight / m_FramesWidth) : 0;

		// Top Fields
		for (int i = 0; i < 11; i += 2) {
			if (m_Fields[i] != nullptr) {
				DrawField(i, fieldDisplayWidth, fieldDisplayHeight);
			}
		}

		// Bottom Fields
		for (int i = 1; i < 10; i += 2) {
			if (m_Fields[i] != nullptr) {
				DrawField(i, fieldDisplayWidth, fieldDisplayHeight);
			}
		}

		ImGui::End();

		ImGui::Begin("Output");

		availableSpace = ImGui::GetContentRegionAvail();
		float frameDisplayWidth = availableSpace.x / 4.1f; // TODO maybe tables make this more precise?
		float frameDisplayHeight = m_FramesWidth ? frameDisplayWidth * ((float)m_FramesHeight / m_FramesWidth) : 0;

		for (int i = 0; i < 4; i++) {
			if (m_Frames[i] == nullptr) {
				continue;
			}

			int activeFrame = m_ActiveCycle * 4 + i;
			if (activeFrame >= m_FramesFrameCount) {
				break;
			}

			if (m_NeedNewFields && !error) {
				const VSFrame* frame = m_VSAPI->getFrame(activeFrame, m_FramesNode, error_message, sizeof(error_message));
				if (!frame) {
					fprintf(stderr, error_message);
					error = 1;
				}
				const VSMap* props = m_VSAPI->getFramePropertiesRO(frame);
				int err = 0;
				const char* freezeFrameProp = m_VSAPI->mapGetData(props, "IVTCDN_FreezeFrame", 0, &err);
				std::string freezeFrame = err ? "" : freezeFrameProp;
				const uint8_t* frame_ptr = m_VSAPI->getReadPtr(frame, 0);
				m_Frames[i]->SetData(frame_ptr);
				m_FreezeFrames[i] = freezeFrame;
				m_VSAPI->freeFrame(frame);
			} else {
				// Sleep for vsync? minimized window uses 100% of 1 CPU core since this is a busy wait without any actions
			}
			DrawFrame(i, frameDisplayWidth, frameDisplayHeight);
		}

		ImGui::End();

		ImGui::Begin("Controls");
		ImGui::SliderInt("Active Cycle", &m_ActiveCycle, 0, max_cycle);
		ImGui::SameLine(); HelpMarker("CTRL+click to input value.");

		ImGui::End();
		//ImGui::ShowDemoWindow();

		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
			if (m_ProjectFile.empty()) {
				SaveProjectAsDialog();
			} else {
				SaveJson();
			}
		}

		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
			OpenProjectDialog();
		}

		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
			NewProjectDialog();
		}

		m_NeedNewFields = false;
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
	}

	void OpenProjectDialog() {
		auto path = IGFD::Utils::ParsePathFileName(m_ProjectFile);
		ImGuiFileDialog::Instance()->OpenModal("OpenProjectDialog", "Choose project file", ".ivtc", path.path + "/.");
	}

	void NewProjectDialog() {
		auto path = IGFD::Utils::ParsePathFileName(m_ProjectFile);
		ImGuiFileDialog::Instance()->OpenModal("NewProjectDialog", "Choose script file", ".vpy", path.path + "/.");
	}

	void SaveProjectAsDialog() {
		auto path = IGFD::Utils::ParsePathFileName(m_ProjectFile);
		ImGuiFileDialog::Instance()->OpenModal("SaveProjectAsDialog", "Choose project file", ".ivtc", path.path + "/.");
	}

	void OpenProject(const char* project_path_name) {
		m_ProjectFile = std::string(project_path_name);
		std::ifstream input(m_ProjectFile, std::ios::binary | std::ios::ate);
		int inputSize = input.tellg();
		input.seekg(0, std::ios::beg);

		std::vector<char> compressed;
		compressed.resize(inputSize);
		input.read(compressed.data(), inputSize);
		std::string decompressed = gzip::decompress(compressed.data(), inputSize);
		m_JsonProps = json::parse(decompressed);
		m_ActiveCycle = m_JsonProps["project_garbage"]["active_cycle"];
		std::string script_file = m_JsonProps["project_garbage"]["script_file"];
		SetActiveFields(script_file.c_str(), true);
	}

	void StartNewProject(const char* script_path_name) {
		static int actions[] = { 0, 1, 2, 3, 8, 5, 4, 8, 6, 7 };
		static std::string notes[] = { "A", "A", "B", "B", "B", "C", "C", "D", "D", "D" };
		m_ProjectFile = "";
		m_JsonProps = R"({
			"ivtc_actions": [],
			"notes": [],
			"scene_changes": [],
			"no_match_handling": {},
			"project_garbage": {}
		})"_json;
		m_JsonProps["project_garbage"]["script_file"] = script_path_name;
		SetActiveFields(script_path_name, false);
		const VSVideoInfo* vi = m_VSAPI->getVideoInfo(m_FieldsNode);
		for (int i = 0; i < vi->numFrames; i++) {
			m_JsonProps["ivtc_actions"][i] = actions[i % 10];
			m_JsonProps["notes"][i] = notes[i % 10];
		}
		LoadFrames();
	}

	void SaveJson() {
		if (m_ProjectFile.empty()) {
			return;
		}

		m_JsonProps["project_garbage"]["active_cycle"] = m_ActiveCycle;
		std::string input = m_JsonProps.dump();
		std::string compressed = gzip::compress(input.c_str(), input.size());
		std::ofstream output(m_ProjectFile, std::ios::binary);
		output << compressed;
	}

private:
	const VSAPI* m_VSAPI = nullptr;
	const VSSCRIPTAPI* m_VSSAPI = nullptr;
	std::string m_ProjectFile = "";
	json m_JsonProps;

	int m_ActiveCycle = 0;
	bool m_NeedNewFields = false;

	// Fields
	VSScript* m_FieldsScriptEnvironment = nullptr;
	VSNode* m_FieldsNode = nullptr;
	int m_FieldsWidth = 0;
	int m_FieldsHeight = 0;
	int m_FieldsFrameCount = 0;
	std::shared_ptr<Walnut::Image> m_Fields[11] = {};

	// Frames
	VSNode* m_FramesNode = nullptr;
	int m_FramesWidth = 0;
	int m_FramesHeight = 0;
	int m_FramesFrameCount = 0;
	std::shared_ptr<Walnut::Image> m_Frames[4] = {};
	std::string m_FreezeFrames[4] = {};

	VSNode* SeparateFields(VSCore* core, VSNode* &node) {
		VSMap* argument_map = m_VSAPI->createMap();
		VSPlugin* std_plugin = m_VSAPI->getPluginByID("com.vapoursynth.std", core);
		m_VSAPI->mapConsumeNode(argument_map, "clip", node, maReplace);
		VSMap* result_map = m_VSAPI->invoke(std_plugin, "SeparateFields", argument_map);

		const char* result_error = m_VSAPI->mapGetError(result_map);
		if (result_error) {
			fprintf(stderr, "%s\n", result_error);
		}

		VSNode* output = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
		return output;
	}

	VSNode* ConvertToRGB(VSCore* core, VSNode* &node) {
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

		VSNode* output = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
		return output;
	}

	VSNode* ShufflePlanes(VSCore* core, VSNode* &node) {
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

		VSNode* output = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
		return output;
	}

	VSNode* Pack(VSCore* core, VSNode* &node) {
		VSMap* argument_map = m_VSAPI->createMap();
		VSPlugin* libp2p_plugin = m_VSAPI->getPluginByID("com.djatom.libp2p", core);
		m_VSAPI->mapConsumeNode(argument_map, "clip", node, maReplace);
		VSMap* result_map = m_VSAPI->invoke(libp2p_plugin, "Pack", argument_map);

		const char* result_error = m_VSAPI->mapGetError(result_map);
		if (result_error) {
			fprintf(stderr, "%s\n", result_error);
		}

		VSNode* output = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
		return output;
	}

	VSNode* IVTCDN(VSCore* core, VSNode* node) {
		VSMap* argument_map = m_VSAPI->createMap();
		VSPlugin* ivtcdn_plugin = m_VSAPI->getPluginByID("tools.mike.ivtc", core);
		m_VSAPI->mapConsumeNode(argument_map, "clip", node, maReplace);
		std::string rawProps = m_JsonProps.dump();
		m_VSAPI->mapSetData(argument_map, "projectfile", rawProps.c_str(), rawProps.size(), dtUtf8, maReplace);
		m_VSAPI->mapSetInt(argument_map, "rawproject", 1, maReplace);
		VSMap* result_map = m_VSAPI->invoke(ivtcdn_plugin, "IVTC", argument_map);

		const char* result_error = m_VSAPI->mapGetError(result_map);
		if (result_error) {
			fprintf(stderr, "%s\n", result_error);
		}

		VSNode* output = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
		return output;
	}

	void DrawField(const int i, const float display_width, const float display_height) {
		int activeField = m_ActiveCycle * 10 + i;
		if (activeField >= m_FieldsFrameCount) {
			return;
		}
		ImVec2 pos = ImGui::GetCursorScreenPos();
		//ImGui::Image(m_Fields[i]->GetDescriptorSet(), { (float)m_FieldsWidth, (float)m_FieldsHeight });
		ImGui::Image(m_Fields[i]->GetDescriptorSet(), { display_width, display_height });
		auto& note = m_JsonProps["notes"][activeField];
		auto& action = m_JsonProps["ivtc_actions"][activeField];
		auto& scene_changes = m_JsonProps["scene_changes"];
		if (ImGui::IsItemHovered()) {
			if (ImGui::IsKeyPressed(ImGuiKey_S) && !ImGui::GetIO().KeyCtrl) {
				auto it = std::find(scene_changes.begin(), scene_changes.end(), activeField);
				if (it == scene_changes.end()) {
					scene_changes.push_back(activeField);
				} else {
					scene_changes.erase(it);
				}
			}

			if (ImGui::IsKeyPressed(ImGuiKey_A)) {
				note = "A";
			} else if (ImGui::IsKeyPressed(ImGuiKey_B)) {
				note = "B";
			} else if (ImGui::IsKeyPressed(ImGuiKey_C)) {
				note = "C";
			} else if (ImGui::IsKeyPressed(ImGuiKey_D)) {
				note = "D";
			}

			const int fieldOffset = i % 2;
			static const int drop = 8;
			if (ImGui::IsKeyPressed(ImGuiKey_1) && i < 11) {
				int positiveAction = 0 + i % 2;
				action = action == positiveAction ? drop : positiveAction;
			} else if (ImGui::IsKeyPressed(ImGuiKey_2) && i < 11) {
				int positiveAction = 2 + i % 2;
				action = action == positiveAction ? drop : positiveAction;
			} else if (ImGui::IsKeyPressed(ImGuiKey_3) && i < 11) {
				int positiveAction = 4 + i % 2;
				action = action == positiveAction ? drop : positiveAction;
			} else if (ImGui::IsKeyPressed(ImGuiKey_4)) {
				if (i < 10) {
					int positiveAction = 6 + i % 2;
					action = action == positiveAction ? drop : positiveAction;
				} else {
					int positiveAction = 9;
					action = action == positiveAction ? drop : positiveAction;
				}
			}
			ImGui::BeginTooltip();
			ImGui::Text("Field %d", i);
			ImGui::EndTooltip();
		}
		if (std::find(scene_changes.begin(), scene_changes.end(), activeField) != scene_changes.end()) {
			ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x - 5, pos.y), ImVec2(pos.x, pos.y + 300), IM_COL32(255, 128, 0, 255));
		}
		ImVec2 textSize = g_UbuntuMonoFont->CalcTextSizeA(64.0f, FLT_MAX, 0.0f, note.get<std::string>().c_str());
		ImVec2 textPos(pos.x + display_width / 2 - textSize.x / 2, pos.y + display_height / 2 - textSize.y / 2);
		ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(textPos.x - 4, textPos.y + 5), ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y), ColorForAction(action.get<int_fast8_t>()));
		ImGui::GetWindowDrawList()->AddText(g_UbuntuMonoFont, 64.0f, textPos, IM_COL32_WHITE, note.get<std::string>().c_str());
		//std::string id = std::format("field{}", i);
		//ImGui::PushID(id.c_str());
		//if (ImGui::BeginPopupContextItem(id.c_str())) {
		//	ImGui::Text("This a popup for field %d!", i);
		//	ImGui::EndPopup();
		//}
		//ImGui::PopID();
		if (i != 10 && (m_FieldsFrameCount - activeField) > 2) {
			ImGui::SameLine();
		}
	}

	void DrawFrame(const int i, const float display_width, const float display_height) {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::Image(m_Frames[i]->GetDescriptorSet(), { display_width, display_height });
		if (ImGui::IsItemHovered()) {
			auto activeFrame = std::to_string(m_ActiveCycle * 4 + i);
			if (ImGui::IsKeyPressed(ImGuiKey_F)) {
				auto& no_match_handling = m_JsonProps["no_match_handling"];
				if (no_match_handling.contains(activeFrame)) {
					no_match_handling.erase(activeFrame);
				} else {
					no_match_handling[activeFrame] = "Next";
				}
			}

			ImGui::BeginTooltip();
			ImGui::Text("Frame %d", i);
			ImGui::EndTooltip();
		}
		auto freezeFrame = m_FreezeFrames[i];
		auto action = freezeFrame.empty() ? i * 2 : 8;
		ImGui::GetWindowDrawList()->AddRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + display_width, pos.y + display_height), ColorForAction(action), 0, 0, 4);
		if (!freezeFrame.empty()) {
			ImVec2 textSize = g_UbuntuMonoFont->CalcTextSizeA(64.0f, FLT_MAX, 0.0f, freezeFrame.c_str());
			ImVec2 textPos(pos.x + display_width / 2 - textSize.x / 2, pos.y + display_height / 2 - textSize.y / 2);
			ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(textPos.x - 4, textPos.y + 5), ImVec2(textPos.x + textSize.x, textPos.y + textSize.y), ColorForAction(action));
			ImGui::GetWindowDrawList()->AddText(g_UbuntuMonoFont, 64.0f, textPos, IM_COL32_WHITE, freezeFrame.c_str());
		}
		//std::string id = std::format("frame{}", i);
		//ImGui::PushID(id.c_str());
		//ImGui::PopID();
		if (i < 4) {
			ImGui::SameLine();
		}
	}

	void ApplyCycleToScene() {
		auto& notes = m_JsonProps["notes"];
		auto& actions = m_JsonProps["ivtc_actions"];
		const auto& scene_changes = m_JsonProps["scene_changes"];

		int start_of_cycle = m_ActiveCycle * 10;
		int end_of_cycle = start_of_cycle + 9;
		std::cerr << "Cycle [" << start_of_cycle << ", " << end_of_cycle << "]" << std::endl;

		int start_of_scene = 0;
		int end_of_scene = m_FieldsFrameCount - 1;
		for (auto it = scene_changes.begin(); it != scene_changes.end(); it++) {
			auto val = it.value();
			if (val > start_of_scene && val < start_of_cycle) {
				start_of_scene = val;
			}
			if (val < end_of_scene && val > start_of_cycle) {
				end_of_scene = val;
			}
		}
		std::cerr << "Scene [" << start_of_scene << ", " << end_of_scene << "]" << std::endl;

		// TODO need to think about cycles a lot
		int_fast8_t cycle_actions[10];
		std::string cycle_notes[10];
		int position_in_cycle = 0;
		for (int i = start_of_cycle; i <= end_of_cycle; i++) {
			cycle_actions[position_in_cycle] = actions[i];
			cycle_notes[position_in_cycle] = notes[i];
			++position_in_cycle;
		}

		// TODO need to think about cycles a lot
		position_in_cycle = start_of_scene % 10;
		for (int i = start_of_scene; i < end_of_scene; i++) {
			actions[i] = cycle_actions[position_in_cycle];
			notes[i] = cycle_notes[position_in_cycle];
			++position_in_cycle %= 10;
		}
	}

	void SetActiveFields(const char* file, bool doLoadFrames=true) {
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
			m_FieldsNode = SeparateFields(core, m_FieldsNode);
			m_FieldsNode = ConvertToRGB(core, m_FieldsNode);
			m_FieldsNode = ShufflePlanes(core, m_FieldsNode);
			m_FieldsNode = Pack(core, m_FieldsNode);
		} else if (vi->format.colorFamily == cfRGB) {
			VSCore* core = m_VSSAPI->getCore(m_FieldsScriptEnvironment);
			m_FieldsNode = ShufflePlanes(core, m_FieldsNode);
			m_FieldsNode = Pack(core, m_FieldsNode);
		} else {
			// Hope for the best?
		}
		vi = m_VSAPI->getVideoInfo(m_FieldsNode);
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

		if (doLoadFrames) {
			LoadFrames();
		}
	}

	void LoadFrames() {
		if (m_FramesNode != nullptr) {
			m_VSAPI->freeNode(m_FramesNode);
		}
		VSNode* rawFieldsNode = m_VSSAPI->getOutputNode(m_FieldsScriptEnvironment, 0);

		const VSVideoInfo* vi = m_VSAPI->getVideoInfo(rawFieldsNode);
		if (vi->format.colorFamily == cfYUV) {
			// Convert to RGB & pack
			VSCore* core = m_VSSAPI->getCore(m_FieldsScriptEnvironment);
			m_FramesNode = SeparateFields(core, rawFieldsNode);
			m_FramesNode = IVTCDN(core, m_FramesNode);
			m_FramesNode = ConvertToRGB(core, m_FramesNode);
			m_FramesNode = ShufflePlanes(core, m_FramesNode);
			m_FramesNode = Pack(core, m_FramesNode);
		} else if (vi->format.colorFamily == cfRGB) {
			// TODO
		} else {
			// Hope for the best?
		}

		vi = m_VSAPI->getVideoInfo(m_FramesNode);
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

		m_NeedNewFields = true;
	}
};

std::shared_ptr<ExampleLayer> g_Layer = nullptr;

void glfw_drop_callback(GLFWwindow* window, int path_count, const char* paths[]) {
	const char* filePathName = paths[0];
	const char* ext = strrchr(filePathName, '.');
	if (!strcmp(ext, ".vpy")) {
		g_Layer->StartNewProject(paths[0]);
	} else if (!strcmp(ext, ".ivtc")) {
		g_Layer->OpenProject(paths[0]);
	}
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
			if (ImGui::MenuItem("New project...", "Ctrl+N")) {
				g_Layer->NewProjectDialog();
			}
			if (ImGui::MenuItem("Open project...", "Ctrl+O")) {
				g_Layer->OpenProjectDialog();
			}
			if (ImGui::MenuItem("Save project", "Ctrl+S")) {
				g_Layer->SaveJson();
			}
			if (ImGui::MenuItem("Save project as...")) {
				g_Layer->SaveProjectAsDialog();
			}
			if (ImGui::MenuItem("Exit")) {
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	glfwSetDropCallback(app->GetWindowHandle(), glfw_drop_callback);
	return app;
}