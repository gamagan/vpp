#include <vpp/vulkan/enums.hpp>

namespace vpp
{

namespace fwd
{

//from context.hpp
const auto defaultDebugFlags = vk::DebugReportBitsEXT::error | vk::DebugReportBitsEXT::warning;

//from commandBuffer.hpp
const auto commandBufferLevelPrimary = vk::CommandBufferLevel::primary;

//pipeline.hpp
const auto allShaderStages = vk::ShaderStageBits::all;

} //fwd

} //vpp