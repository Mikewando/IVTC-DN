#include <GLFW/glfw3.h> // For drag-n-drop files

#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "vapoursynth/VSScript4.h"
#include "vapoursynth/VSHelper4.h"

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

class ExampleLayer : public Walnut::Layer
{
public:
	virtual void OnUIRender() override
	{
		static int error = 0;
		static char error_message[1024];

		static int active_cycle = 1336;
		static int last_cycle = -1;
		bool needNewFrames = last_cycle != active_cycle;
		last_cycle = active_cycle;

		int max_cycle = m_FieldsFrameCount / 5;

		if (active_cycle < max_cycle && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
			active_cycle++;
		}
		if (active_cycle > 0 && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
			active_cycle--;
		}
		ImGui::Begin("Fields");

		for (int i = 0; i < 5; i++) {
			if (needNewFrames && !error) {
				const VSFrame* frame = m_VSAPI->getFrame(active_cycle * 5 + i, m_FieldsNode, error_message, sizeof(error_message));
				if (!frame) {
					fprintf(stderr, error_message);
					error = 1;
				}
				const uint8_t* frame_ptr = m_VSAPI->getReadPtr(frame, 0);
				m_Fields[i]->SetData(frame_ptr);
				m_VSAPI->freeFrame(frame);
			}
		}
		for (int i = 0; i < 5; i++) {
			ImGui::Image(m_Fields[i]->GetDescriptorSet(), { (float)m_FieldsWidth, (float)m_FieldsHeight });
			if (i < 4) {
				ImGui::SameLine();
			}
		}

		ImGui::End();

		ImGui::Begin("Output");

		for (int i = 0; i < 4; i++) {
			if (needNewFrames && !error) {
				const VSFrame* frame = m_VSAPI->getFrame(active_cycle * 4 + i, m_FramesNode, error_message, sizeof(error_message));
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
		ImGui::SliderInt("Active Cycle", &active_cycle, 0, max_cycle);
		ImGui::SameLine(); HelpMarker("CTRL+click to input value.");
		static bool frames[10] = {true, true, true, true, false, true, true, false, true, true};
		int active_count = 0;
		for (int i = 0; i < 10; i++) {
			if (frames[i]) {
				active_count++;
			}
			if (active_count > 8) {
				frames[i] = false;
			}
		}

		// Top Fields
		ImGui::Checkbox("0", &frames[0]); ImGui::SameLine();
		ImGui::Checkbox("2", &frames[2]); ImGui::SameLine();
		ImGui::Checkbox("4", &frames[4]); ImGui::SameLine();
		ImGui::Checkbox("6", &frames[6]); ImGui::SameLine();
		ImGui::Checkbox("8", &frames[8]);

		// Bottom Fields
		ImGui::Checkbox("1", &frames[1]); ImGui::SameLine();
		ImGui::Checkbox("3", &frames[3]); ImGui::SameLine();
		ImGui::Checkbox("5", &frames[5]); ImGui::SameLine();
		ImGui::Checkbox("7", &frames[7]); ImGui::SameLine();
		ImGui::Checkbox("9", &frames[9]);

		ImGui::End();
		ImGui::ShowDemoWindow();
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

		m_ScriptEnvironment = m_VSSAPI->createScript(nullptr);

		set_active_fields("A:\\Subs\\todo\\flcl\\01\\walnut\\separate_fields.vpy");
		set_active_frames("A:\\Subs\\todo\\flcl\\01\\walnut\\output frames.vpy");
	}

	void set_active_fields(const char* file) {
		int error = m_VSSAPI->evaluateFile(m_ScriptEnvironment, file);
		if (error != 0) {
			fprintf(stderr, "Error loading file: %s\n", m_VSSAPI->getError(m_ScriptEnvironment));
		}

		m_FieldsNode = m_VSSAPI->getOutputNode(m_ScriptEnvironment, 0);
		const VSVideoInfo* vi = m_VSAPI->getVideoInfo(m_FieldsNode);
		fprintf(stderr, "Video Width: %d x %d\n", vi->width, vi->height);
		fprintf(stderr, "Video Format: %d, %d, %d\n", vi->format.colorFamily, vi->format.bitsPerSample, vi->format.numPlanes);
		m_FieldsWidth = vi->width;
		m_FieldsHeight = vi->height;
		m_FieldsFrameCount = vi->numFrames;

		for (int i = 0; i < 5; i++) {
			m_Fields[i] = std::make_shared<Walnut::Image>(
				m_FieldsWidth,
				m_FieldsHeight,
				Walnut::ImageFormat::RGBA,
				nullptr);
		}
	}

	void set_active_frames(const char* file) {
		int error = m_VSSAPI->evaluateFile(m_ScriptEnvironment, file);
		if (error != 0) {
			fprintf(stderr, "Error loading file: %s\n", m_VSSAPI->getError(m_ScriptEnvironment));
		}

		m_FramesNode = m_VSSAPI->getOutputNode(m_ScriptEnvironment, 0);
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
	}


private:
	const VSAPI* m_VSAPI = nullptr;
	const VSSCRIPTAPI* m_VSSAPI = nullptr;
	VSScript* m_ScriptEnvironment = nullptr;
	const char* m_ActiveFile; // TODO
	// Fields
	VSNode* m_FieldsNode = nullptr;
	int m_FieldsWidth = 0;
	int m_FieldsHeight = 0;
	int m_FieldsFrameCount = 0;
	std::shared_ptr<Walnut::Image> m_Fields[5];
	// Frames
	VSNode* m_FramesNode = nullptr;
	int m_FramesWidth = 0;
	int m_FramesHeight = 0;
	int m_FramesFrameCount = 0;
	std::shared_ptr<Walnut::Image> m_Frames[4];
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
	spec.Name = "IVTC Deez";

	Walnut::Application* app = new Walnut::Application(spec);
	//app->PushLayer<ExampleLayer>();
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