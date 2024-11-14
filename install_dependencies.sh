rm -rf ./InstallVulkan.app || true
rm vulkansdk-macos-1.3.296.0.zip || true
sudo rm -rf SDK || true

wget https://sdk.lunarg.com/sdk/download/1.3.296.0/mac/vulkansdk-macos-1.3.296.0.zip
unzip vulkansdk-macos-1.3.296.0.zip
sudo ./InstallVulkan.app/Contents/MacOS/InstallVulkan --root "$PWD/SDK" --accept-licenses --default-answer --confirm-command install com.lunarg.vulkan.core com.lunarg.vulkan.sdl2 com.lunarg.vulkan.glm com.lunarg.vulkan.volk com.lunarg.vulkan.vma

rm -rf ./InstallVulkan.app || true
rm vulkansdk-macos-1.3.296.0.zip || true