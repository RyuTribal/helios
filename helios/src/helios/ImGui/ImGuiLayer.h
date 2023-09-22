#pragma once

#include "helios/Layer.h"
#include "helios/events/ApplicationEvent.h"
#include "helios/events/KeyEvent.h"
#include "helios/events/MouseEvent.h"

namespace Helios
{
	class ImGuiLayer: public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnImGuiRender() override;

		void Begin();
		void End();

	private:
		float m_Time = 0.0f;

	};
}
