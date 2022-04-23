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
		ImGui::Begin("Preview");

		static int error = 0;
		static int active_frame = 3746;
		char error_message[1024];
		if (!error) {
			const VSFrame* frame = m_VSAPI->getFrame(active_frame, m_Node, error_message, sizeof(error_message));
			if (!frame) {
				fprintf(stderr, error_message);
				error = 1;
			}
			const uint8_t* frame_ptr = m_VSAPI->getReadPtr(frame, 0);
			m_Image->SetData(frame_ptr);
			m_VSAPI->freeFrame(frame);
		}
		ImGui::Image(m_Image->GetDescriptorSet(), { (float)m_Width, (float)m_Height });

		ImGui::End();

		ImGui::Begin("Controls");
		ImGui::SliderInt("Active Frame", &active_frame, 0, m_FrameCount - 1);
		ImGui::SameLine(); HelpMarker("CTRL+click to input value.");
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

		set_active_file("A:\\Subs\\todo\\flcl\\01\\walnut.vpy");
	}

	void set_active_file(const char* file) {
		int error = m_VSSAPI->evaluateFile(m_ScriptEnvironment, file);
		if (error != 0) {
			fprintf(stderr, "Error loading file: %s\n", m_VSSAPI->getError(m_ScriptEnvironment));
		}

		m_Node = m_VSSAPI->getOutputNode(m_ScriptEnvironment, 0);
		m_VI = m_VSAPI->getVideoInfo(m_Node);
		fprintf(stderr, "Video Width: %d x %d\n", m_VI->width, m_VI->height);
		fprintf(stderr, "Video Format: %d, %d, %d\n", m_VI->format.colorFamily, m_VI->format.bitsPerSample, m_VI->format.numPlanes);
		m_Width = m_VI->width;
		m_Height = m_VI->height;
		m_FrameCount = m_VI->numFrames;

		m_Image = std::make_shared<Walnut::Image>(
			m_Width,
			m_Height,
			Walnut::ImageFormat::RGBA,
			nullptr);
		m_ActiveFile = file;
	}

private:
	const VSAPI* m_VSAPI = nullptr;
	const VSSCRIPTAPI* m_VSSAPI = nullptr;
	VSScript* m_ScriptEnvironment = nullptr;
	VSNode* m_Node = nullptr;
	const VSVideoInfo* m_VI = nullptr;
	int m_Width = 0;
	int m_Height = 0;
	int m_FrameCount = 0;
	const char* m_ActiveFile;
	std::shared_ptr<Walnut::Image> m_Image;

};

std::shared_ptr<ExampleLayer> g_Layer = nullptr;

void glfw_drop_callback(GLFWwindow* window, int path_count, const char* paths[]) {
	fprintf(stderr, "Path count: %d, First Path: %s\n", path_count, paths[0]);
	g_Layer->set_active_file(paths[0]);
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