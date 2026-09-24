// PerfNetHelpers.h
#pragma once

#include "stdafx.h"

// TRUE if sa is an IPv6 link-local address (fe80::/10).
BOOL PerfNet_IsIPv6LinkLocal(const SOCKADDR* sa);
