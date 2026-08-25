# Skyrim Fitting System v1.4.8 — Third-Party Notices

SFSCore is distributed under the GNU General Public License, version 3.0. A
copy is provided in `LICENSE`.

The following components are incorporated into, or linked by, SFSCore. Their
license notices are reproduced below.

## GPL-3.0-or-later component

### CommonLibSSE-NG v6.7.0

Copyright (c) 2018 Ryan-rsm-McKenzie.

The exact source used for SFS v1.4.8 is included at
`third_party/CommonLibSSE-NG`, upstream commit
`3d81614617910e7f34b33d8750881811b5e36445` from
https://github.com/alandtse/CommonLibSSE-NG (branch `ng`). Its `COPYING` and
`EXCEPTIONS.md` files are included in the corresponding source package.

## MIT-licensed components

### Dear ImGui

Copyright (c) 2014-2026 Omar Cornut. Source is included under `lib/imgui`.

### nlohmann/json

Copyright (c) 2013-2026 Niels Lohmann. Version `3.12.0` is pinned in
`xmake-requires.lock`.

### spdlog

Copyright (c) 2016 Gabi Melman.

### SimpleIni

Copyright (c) 2006-2013 Brodie Thiesfield.

### toml11

Copyright (c) 2017-2025 Toru Niina.

### DirectXMath and DirectX Tool Kit

Copyright (c) Microsoft Corporation.

The MIT License applies to each component in this section:

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to
> deal in the Software without restriction, including without limitation the
> rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
> sell copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
> FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
> IN THE SOFTWARE.

## BSD 3-Clause components

### rapidcsv

Copyright (c) 2017, Kristofer Berggren. All rights reserved.

### Xbyak

Copyright (c) 2007-2026 Masayuki Saito. All rights reserved.

The BSD 3-Clause License applies to each component in this section:

> Redistribution and use in source and binary forms, with or without
> modification, are permitted provided that the following conditions are met:
>
> 1. Redistributions of source code must retain the above copyright notice,
>    this list of conditions and the following disclaimer.
> 2. Redistributions in binary form must reproduce the above copyright notice,
>    this list of conditions and the following disclaimer in the documentation
>    and/or other materials provided with the distribution.
> 3. Neither the name of the copyright holder nor the names of its contributors
>    may be used to endorse or promote products derived from this software
>    without specific prior written permission.
>
> THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
> AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
> IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
> ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
> LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
> CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
> SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
> INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
> CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
> ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
> POSSIBILITY OF SUCH DAMAGE.

## Runtime dependencies and build tools

SKSE64 (the SKSE Team) and Address Library for SKSE Plugins (meh321) are
required runtime dependencies but are not included in SFS. XMake is an
Apache-2.0 build tool used to compile the source and is not distributed with
the runtime package. These projects are credited in the mod description; their
own licenses and distribution terms remain with their respective projects.
