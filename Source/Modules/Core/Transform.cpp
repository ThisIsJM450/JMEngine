#include "Transform.h"

const DirectX::XMVECTORF32 Transform::gForward = { 0.f, 0.f, 1.f, 0.f };
const DirectX::XMVECTORF32 Transform::gRight   = { 1.f, 0.f, 0.f, 0.f };
const DirectX::XMVECTORF32 Transform::gUp      = { 0.f, 1.f, 0.f, 0.f };