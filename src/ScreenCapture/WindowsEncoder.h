#pragma once
#include "VideoEncoder.h"

std::unique_ptr<VideoEncoderImpl> CreateWindowsEncoder();
