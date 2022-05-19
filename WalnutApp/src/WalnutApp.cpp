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
#include "gzip/compress.hpp"
#include "gzip/decompress.hpp"

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

static ImU32 ColorForAction(const std::string& action) {
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
			//set_active_frames(m_ActiveFile);
			set_active_fields(m_ActiveFile);
			//load_frames();
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
			} else {
				// Sleep for vsync? minimized window uses 100% of 1 CPU core since this is a busy wait without any actions
			}
		}

		for (int i = 0; i < 4; i++) {
			ImGui::Image(m_Frames[i]->GetDescriptorSet(), { (float)m_FramesWidth, (float)m_FramesHeight });
			if (ImGui::IsItemHovered()) {
				auto activeFrame = std::to_string(m_ActiveCycle * 4 + i);
				if (ImGui::IsKeyPressed(ImGuiKey_N)) {
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
			std::string id = std::format("frame{}", i);
			//ImGui::PushID(id.c_str());
			//ImGui::PopID();
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

		//std::ifstream props_file("A:\\Subs\\todo\\flcl\\01\\walnut\\props_v2.json");
		//std::ifstream props_file(m_JsonFile);
		//props_file >> m_JsonProps;
		std::ifstream input(m_JsonFile, std::ios::binary | std::ios::ate);
		int inputSize = input.tellg();
		input.seekg(0, std::ios::beg);

		std::vector<char> compressed;
		compressed.resize(inputSize);
		input.read(compressed.data(), inputSize);
		std::string decompressed = gzip::decompress(compressed.data(), inputSize);
		m_JsonProps = json::parse(decompressed);

		//set_active_fields("A:\\Subs\\todo\\flcl\\01\\walnut\\separate_fields.vpy");
		//set_active_fields("A:\\Subs\\todo\\garzey\\01\\line doubling\\separate fields.vpy");
		set_active_fields(R"(A:\Subs\todo\otaku no video\01\walnut\fields.vpy)");
		//set_active_frames("A:\\Subs\\todo\\flcl\\01\\walnut\\output frames.vpy");
		//set_active_frames(R"(A:\Subs\todo\otaku no video\01\walnut\output.vpy)");
	}

//	void set_active_frames(const char* file) {
//		if (m_FramesScriptEnvironment != nullptr) {
//			m_VSAPI->freeNode(m_FramesNode);
//			m_VSSAPI->freeScript(m_FramesScriptEnvironment);
//		}
//		m_FramesScriptEnvironment = m_VSSAPI->createScript(nullptr);
//
//		m_VSSAPI->evalSetWorkingDir(m_FramesScriptEnvironment, 1);
//		int error = m_VSSAPI->evaluateFile(m_FramesScriptEnvironment, file);
//		if (error != 0) {
//			fprintf(stderr, "Error loading file: %s\n", m_VSSAPI->getError(m_FramesScriptEnvironment));
//		}
//
//		m_FramesNode = m_VSSAPI->getOutputNode(m_FramesScriptEnvironment, 0);
//		const VSVideoInfo* vi = m_VSAPI->getVideoInfo(m_FramesNode);
//		if (vi->format.colorFamily == cfYUV) {
//			// Convert to RGB & pack
//			VSCore* core = m_VSSAPI->getCore(m_FramesScriptEnvironment);
//			ConvertToRGB(core, m_FramesNode);
//			ShufflePlanes(core, m_FramesNode);
//			Pack(core, m_FramesNode);
//		} else if (vi->format.colorFamily == cfRGB) {
//			VSCore* core = m_VSSAPI->getCore(m_FramesScriptEnvironment);
//			ShufflePlanes(core, m_FramesNode);
//			Pack(core, m_FramesNode);
//		} else {
//			// Hope for the best?
//		}
//		fprintf(stderr, "Video Width: %d x %d\n", vi->width, vi->height);
//		fprintf(stderr, "Video Format: %d, %d, %d\n", vi->format.colorFamily, vi->format.bitsPerSample, vi->format.numPlanes);
//		m_FramesWidth = vi->width;
//		m_FramesHeight = vi->height;
//		m_FramesFrameCount = vi->numFrames;
//
//		for (int i = 0; i < 4; i++) {
//			m_Frames[i] = std::make_shared<Walnut::Image>(
//				m_FramesWidth,
//				m_FramesHeight,
//				Walnut::ImageFormat::RGBA,
//				nullptr);
//		}
//		m_ActiveFile = file;
//		m_NeedNewFields = true;
//	}

private:
	ImFont* m_UbuntuMonoFont = nullptr;

	const VSAPI* m_VSAPI = nullptr;
	const VSSCRIPTAPI* m_VSSAPI = nullptr;
	const char* m_ActiveFile; // TODO
	const char* m_JsonFile = R"(A:\Subs\todo\otaku no video\01\walnut\props.ivtc)";
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
	VSNode* m_RawFieldsNode = nullptr;
	VSNode* m_FramesNode = nullptr;
	int m_FramesWidth = 0;
	int m_FramesHeight = 0;
	int m_FramesFrameCount = 0;
	std::shared_ptr<Walnut::Image> m_Frames[4];

	void Resize(VSCore* core, VSNode* &node, int64_t width, int64_t height) {
		VSMap* argument_map = m_VSAPI->createMap();
		VSPlugin* resize_plugin = m_VSAPI->getPluginByID("com.vapoursynth.resize", core);
		m_VSAPI->mapConsumeNode(argument_map, "clip", node, maReplace);
		m_VSAPI->mapSetInt(argument_map, "width", width, maReplace);
		m_VSAPI->mapSetInt(argument_map, "height", height, maReplace);
		VSMap* result_map = m_VSAPI->invoke(resize_plugin, "Spline36", argument_map);

		const char* result_error = m_VSAPI->mapGetError(result_map);
		if (result_error) {
			fprintf(stderr, "%s\n", result_error);
		}

		node = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
	}

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

	const VSNode* IVTCDN(VSCore* core, VSNode* node) {
		VSMap* argument_map = m_VSAPI->createMap();
		VSPlugin* ivtcdn_plugin = m_VSAPI->getPluginByID("tools.mike.ivtc", core);
		m_VSAPI->mapConsumeNode(argument_map, "clip", node, maReplace);
		std::string rawProps = m_JsonProps.dump();
		//m_VSAPI->mapSetData(argument_map, "projectfile", m_JsonFile, strlen(m_JsonFile), dtUtf8, maReplace);
		m_VSAPI->mapSetData(argument_map, "projectfile", rawProps.c_str(), rawProps.size(), dtUtf8, maReplace);
		m_VSAPI->mapSetInt(argument_map, "rawproject", 1, maReplace);
		VSMap* result_map = m_VSAPI->invoke(ivtcdn_plugin, "IVTC", argument_map);

		const char* result_error = m_VSAPI->mapGetError(result_map);
		if (result_error) {
			fprintf(stderr, "%s\n", result_error);
		}

		const VSNode* output = m_VSAPI->mapGetNode(result_map, "clip", 0, nullptr);
		m_VSAPI->freeMap(argument_map);
		m_VSAPI->freeMap(result_map);
		return output;
	}

	void DrawField(int i) {
		int active_field = m_ActiveCycle * 10 + i;
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::Image(m_Fields[i]->GetDescriptorSet(), { (float)m_FieldsWidth, (float)m_FieldsHeight });
		auto& note = m_JsonProps["notes"][active_field];
		auto& action = m_JsonProps["ivtc_actions"][active_field];
		auto& scene_changes = m_JsonProps["scene_changes"];
		if (ImGui::IsItemHovered()) {
			if (ImGui::IsKeyPressed(ImGuiKey_S)) {
				auto it = std::find(scene_changes.begin(), scene_changes.end(), active_field);
				if (it == scene_changes.end()) {
					scene_changes.push_back(active_field);
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

			const char* top_or_bottom = i % 2 ? "Bottom" : "Top";
			if (ImGui::IsKeyPressed(ImGuiKey_1) && i < 11) {
				std::string positive_action = std::format("{} Frame 0", top_or_bottom);
				if (action == positive_action) {
					action = "Drop";
				} else {
					action = positive_action;
				}
			} else if (ImGui::IsKeyPressed(ImGuiKey_2) && i < 11) {
				std::string positive_action = std::format("{} Frame 1", top_or_bottom);
				if (action == positive_action) {
					action = "Drop";
				} else {
					action = positive_action;
				}
			} else if (ImGui::IsKeyPressed(ImGuiKey_3) && i < 11) {
				std::string positive_action = std::format("{} Frame 2", top_or_bottom);
				if (action == positive_action) {
					action = "Drop";
				} else {
					action = positive_action;
				}
			} else if (ImGui::IsKeyPressed(ImGuiKey_4)) {
				std::string positive_action;
				if (i < 10) {
					positive_action = std::format("{} Frame 3", top_or_bottom);
				} else {
					positive_action = "Complete Previous Cycle";
				}
				if (action == positive_action) {
					action = "Drop";
				} else {
					action = positive_action;
				}
			}
			ImGui::BeginTooltip();
			ImGui::Text("Field %d", i);
			ImGui::EndTooltip();
		}
		if (std::find(scene_changes.begin(), scene_changes.end(), active_field) != scene_changes.end()) {
			ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x - 5, pos.y), ImVec2(pos.x, pos.y + 300), IM_COL32(255, 128, 0, 255));
		}
		ImVec2 text_pos(pos.x + 184, pos.y + 118);
		ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(text_pos.x - 4, text_pos.y + 5), ImVec2(text_pos.x + 40, text_pos.y + 60), ColorForAction(action.get<std::string>()));
		ImGui::GetWindowDrawList()->AddText(m_UbuntuMonoFont, 64.0f, text_pos, IM_COL32_WHITE, note.get<std::string>().c_str());
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
		//std::ofstream props_file(m_JsonFile);
		//props_file << m_JsonProps;
		std::string input = m_JsonProps.dump();
		std::string compressed = gzip::compress(input.c_str(), input.size());
		std::ofstream output(m_JsonFile, std::ios::binary);
		output << compressed;
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
		std::string cycle_actions[10];
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

	void set_active_fields(const char* file) {
		m_ActiveFile = file;

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
			Resize(core, m_FieldsNode, 400, 300);
			ShufflePlanes(core, m_FieldsNode);
			Pack(core, m_FieldsNode);
		} else if (vi->format.colorFamily == cfRGB) {
			VSCore* core = m_VSSAPI->getCore(m_FieldsScriptEnvironment);
			ShufflePlanes(core, m_FieldsNode);
			Pack(core, m_FieldsNode);
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

		load_frames();
	}
	void load_frames() {
		if (m_FramesScriptEnvironment == nullptr) {
			m_FramesScriptEnvironment = m_VSSAPI->createScript(nullptr);

			m_VSSAPI->evalSetWorkingDir(m_FramesScriptEnvironment, 1);
			int error = m_VSSAPI->evaluateFile(m_FramesScriptEnvironment, m_ActiveFile);
			if (error != 0) {
				fprintf(stderr, "Error loading file: %s\n", m_VSSAPI->getError(m_FramesScriptEnvironment));
			}
			m_RawFieldsNode = m_FramesNode = m_VSSAPI->getOutputNode(m_FramesScriptEnvironment, 0);
		} else {
			//m_VSSAPI->freeScript(m_FramesScriptEnvironment);
			//m_VSAPI->freeNode(m_FramesNode);
		}

		m_FramesNode = m_RawFieldsNode;
		const VSVideoInfo* vi = m_VSAPI->getVideoInfo(m_RawFieldsNode);
		if (vi->format.colorFamily == cfYUV) {
			// Convert to RGB & pack
			VSCore* core = m_VSSAPI->getCore(m_FramesScriptEnvironment);
			IVTCDN(core, m_FramesNode);
			ConvertToRGB(core, m_FramesNode);
			Resize(core, m_FramesNode, 600, 450);
			ShufflePlanes(core, m_FramesNode);
			Pack(core, m_FramesNode);
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

	//void load_frames() {
	//	if (m_FramesScriptEnvironment != nullptr) {
	//		m_VSAPI->freeNode(m_FramesNode);
	//		m_VSSAPI->freeScript(m_FramesScriptEnvironment);
	//	}
	//	m_FramesScriptEnvironment = m_VSSAPI->createScript(nullptr);

	//	m_VSSAPI->evalSetWorkingDir(m_FramesScriptEnvironment, 1);
	//	int error = m_VSSAPI->evaluateFile(m_FramesScriptEnvironment, m_ActiveFile);
	//	if (error != 0) {
	//		fprintf(stderr, "Error loading file: %s\n", m_VSSAPI->getError(m_FramesScriptEnvironment));
	//	}

	//	m_RawFieldsNode = m_FramesNode = m_VSSAPI->getOutputNode(m_FramesScriptEnvironment, 0);
	//	const VSVideoInfo* vi = m_VSAPI->getVideoInfo(m_RawFieldsNode);
	//	if (vi->format.colorFamily == cfYUV) {
	//		// Convert to RGB & pack
	//		VSCore* core = m_VSSAPI->getCore(m_FramesScriptEnvironment);
	//		IVTCDN(core, m_FramesNode);
	//		ConvertToRGB(core, m_FramesNode);
	//		Resize(core, m_FramesNode, 600, 450);
	//		ShufflePlanes(core, m_FramesNode);
	//		Pack(core, m_FramesNode);
	//	} else if (vi->format.colorFamily == cfRGB) {
	//		// TODO
	//	} else {
	//		// Hope for the best?
	//	}

	//	vi = m_VSAPI->getVideoInfo(m_FramesNode);
	//	m_FramesWidth = vi->width;
	//	m_FramesHeight = vi->height;
	//	m_FramesFrameCount = vi->numFrames;

	//	for (int i = 0; i < 4; i++) {
	//		m_Frames[i] = std::make_shared<Walnut::Image>(
	//			m_FramesWidth,
	//			m_FramesHeight,
	//			Walnut::ImageFormat::RGBA,
	//			nullptr);
	//	}

	//	m_NeedNewFields = true;
	//}
};

std::shared_ptr<ExampleLayer> g_Layer = nullptr;

void glfw_drop_callback(GLFWwindow* window, int path_count, const char* paths[]) {
	//g_Layer->set_active_frames(paths[0]);
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