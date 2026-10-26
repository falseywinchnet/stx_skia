/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkFlattenable.h"

// The owned CPU library does not register serialized effect factories.
void SkFlattenable::PrivateInitializer::InitEffects() {}
void SkFlattenable::PrivateInitializer::InitImageFilters() {}
