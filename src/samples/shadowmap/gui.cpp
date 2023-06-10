#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    ImGui::SliderInt("Particle spawn intensity", (int*)&m_particlesSpawnMaxCount, 1, 1000);
    ImGui::SliderFloat("Particle lifetime", &m_particlesLifetime, 1, 10);
    ImGui::SliderFloat("Particle velocity scale", &m_particlesVelocityScale, 0.1, 10.0);
    ImGui::SliderFloat("Particle size", &m_pointSize, 1.0, 10.0);
    ImGui::SliderFloat("Particle acceleration", &m_M, 1.0, 1000.0);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
