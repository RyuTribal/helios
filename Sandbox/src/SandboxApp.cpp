#include <Helios.h>

class ExampleLayer : public Helios::Layer
{
public:
	ExampleLayer() : Layer("Example"){}

	void OnUpdate() override
	{
		if(Helios::Input::IsKeyPressed(HVE_KEY_SPACE))
		{
			HVE_TRACE("The fucken bird jumped");
		}
		//HVE_INFO("ExampleLayer::Update");
	}

	void OnEvent(Helios::Event& event) override
	{
		HVE_TRACE("{0}", event);
	}
};


class Sandbox : public Helios::Application
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

Helios::Application* Helios::CreateApplication()
{
	return new Sandbox();
}