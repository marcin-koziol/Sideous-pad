dpf_add_plugin(sideous-pad
  TARGETS lv2 vst3 clap jack
  UI_TYPE cairo
  FILES_DSP
      SideousPadPlugin.cpp
  FILES_UI
      SideousPadUI.cpp)

target_include_directories(sideous-pad PUBLIC ".")
