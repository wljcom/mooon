/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: jian yi, eyjian@qq.com
 */
#ifndef MOOON_UTILS_HASH_SET_H
#define MOOON_UTILS_HASH_SET_H

/***
  * 处理stdext和tr1中hash_set兼容问题，
  */
#ifdef __DEPRECATED
#include <tr1/unordered_set>
#define hash_set std::tr1::unordered_set
#else
#include <ext/hash_set>
#define hash_set __gnu_cxx::hash_set
#endif // __DEPRECATED

#endif // MOOON_UTILS_HASH_SET_H
