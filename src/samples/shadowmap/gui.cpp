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

    ImGui::SliderFloat("Point size", &m_pointSize, 1.0, 10.0);
    ImGui::SliderInt("Instance count", &m_instanceCount, 1, 100000);
    ImGui::SliderInt("Level of detail", &m_lod, 0, 5);
    ImGui::Checkbox("Point cloud rendering", &m_pointRendering);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Rendering %d points", m_pointCount);
    ImGui::Text("Rendering %d polygons", m_polygonCount);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}
