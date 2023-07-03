#include <Helios.h>

class Sandbox : public hve::Application
{
public:
	Sandbox()
	{
		
	}

	~Sandbox()
	{
		
	}


};

hve::Application* hve::CreateApplication()
{
	return new Sandbox();
}