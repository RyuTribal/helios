#include <Helios.h>

class ExampleLayer : public hve::Layer
{
public:
	ExampleLayer() : Layer("Example"){}

	void OnUpdate() override
	{
		HVE_INFO("ExampleLayer::Update");
	}

	void OnEvent(hve::Event& event) override
	{
		HVE_TRACE("{0}", event);
	}
};


class Sandbox : public hve::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
	}

	~Sandbox()
	{
		
	}


};

hve::Application* hve::CreateApplication()
{
	return new Sandbox();
}