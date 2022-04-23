#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "vapoursynth/VSScript4.h"
#include "vapoursynth/VSHelper4.h"

class ExampleLayer : public Walnut::Layer
{
public:
	virtual void OnUIRender() override
	{
		ImGui::Begin("Hello");
		ImGui::Button("Button");

		char error_message[1024];
		if (!error) {
			const VSFrame* frame = vsapi->getFrame(3746, node, error_message, sizeof(error_message));
			if (!frame) {
				fprintf(stderr, error_message);
				error = 1;
			}
			const uint8_t* frame_ptr = vsapi->getReadPtr(frame, 0);
			m_Image->SetData(frame_ptr);
			vsapi->freeFrame(frame);
		}
		ImGui::Image(m_Image->GetDescriptorSet(), { (float)width, (float)height });

		ImGui::End();

		ImGui::ShowDemoWindow();
	}

	ExampleLayer() {
		vssapi = getVSScriptAPI(VSSCRIPT_API_VERSION);
		if (!vssapi) {
			// VapourSynth probably isn't properly installed at all
			fprintf(stderr, "Failed to initialize VSScript library\n");
		}
		assert(vssapi);

		// Get a pointer to the normal api struct, exists so you don't have to link with the VapourSynth core library
		// Failure only happens on very rare API version mismatches and usually doesn't need to be checked
		vsapi = vssapi->getVSAPI(VAPOURSYNTH_API_VERSION);
		assert(vsapi);

		se = vssapi->createScript(nullptr);
		int error = vssapi->evaluateFile(se, "A:\\Subs\\todo\\flcl\\01\\walnut.vpy");
		if (error != 0) {
			fprintf(stderr, vssapi->getError(se));
		}
		node = vssapi->getOutputNode(se, 0);
		vi = vsapi->getVideoInfo(node);
		fprintf(stderr, "Video Width: %d x %d\n", vi->width, vi->height);
		fprintf(stderr, "Video Format: %d, %d, %d\n", vi->format.colorFamily, vi->format.bitsPerSample, vi->format.numPlanes);
		width = vi->width;
		height = vi->height;

		m_Image = std::make_shared<Walnut::Image>(
			width,
			height,
			Walnut::ImageFormat::RGBA,
			nullptr);
		// This size calculation is from vsjs and I didn't comment it...
		//size_t frame_size = (vi->width * vi->format.bytesPerSample) >> vi->format.subSamplingW;
		//if (frame_size) {
		//	frame_size *= vi->height;
		//	frame_size >>= vi->format.subSamplingH;
		//	frame_size *= 2;
		//}
		//frame_size += vi->width * vi->format.bytesPerSample * vi->height;

		//frame_buffer = (uint8_t*)malloc(frame_size);
	}
private:
	const VSAPI* vsapi = nullptr;
	const VSSCRIPTAPI* vssapi = nullptr;
	VSScript* se = nullptr;
	VSNode* node = nullptr;
	const VSVideoInfo* vi = nullptr;
	int error = 0;
	int width = 0;
	int height = 0;
	std::shared_ptr<Walnut::Image> m_Image;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Walnut Example";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
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
	return app;
}