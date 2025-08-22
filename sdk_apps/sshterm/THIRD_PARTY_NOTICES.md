Third-Party Notices and Attributions

This application incorporates third-party components. Their licenses and attributions are listed here to satisfy legal requirements. The GPLv3 terms for this application are in LICENSE and the full text is in the repository root file COPYING.

Summary
- libvterm 0.3.3 — MIT License
- SDL3 — zlib License
- Leggie 9x18 font — Creative Commons Attribution 4.0 International (CC BY 4.0)
- wolfSSH 1.4.20 — GPLv3 License
- wolfSSL 5.8.2 — GPLv3 License
- ESP-IDF sockets/lwIP — Apache License 2.0 (provided by the firmware SDK)

Details

1) libvterm 0.3.3 (MIT)
- Upstream: https://www.leonerd.org.uk/code/libvterm/
- Bundled location: thirdparty/libvterm-0.3.3/
- License file: thirdparty/libvterm-0.3.3/LICENSE
- Copyright (c) 2008 Paul Evans <leonerd@leonerd.org.uk>
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

2) SDL3 (zlib)
- Upstream: https://libsdl.org/
- Provided by: sdk_libs/sdl3/SDL3/
- License file: sdk_libs/sdl3/SDL3/LICENSE.txt
This software is provided 'as-is', without any express or implied warranty. Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions: (1) The origin of this software must not be misrepresented; (2) Altered source versions must be plainly marked as such; (3) This notice may not be removed or altered from any source distribution.

3) Leggie 9x18 font (CC BY 4.0)
- Author: Wiktor Kerr — "Leggie" bitmap font
- License: Creative Commons Attribution 4.0 International (CC BY 4.0)
- Source: https://memleek.org/leggie
- Usage: This app embeds a subset for ASCII 32-126 in components/renderer/font_leggie_9x18.h
- Required attribution: "Leggie is Copyright © 2012-2018 Wiktor Kerr. Licensed under CC BY 4.0."
- CC BY 4.0 summary: https://creativecommons.org/licenses/by/4.0/
- Full legal code: https://creativecommons.org/licenses/by/4.0/legalcode

4) wolfSSH 1.4.20 (GPLv3)
- Upstream: https://www.wolfssl.com/products/wolfssh/
- GitHub: https://github.com/wolfSSL/wolfssh
- Bundled location: thirdparty/wolfssh-1.4.20/
- License file: thirdparty/wolfssh-1.4.20/COPYING
- Copyright (C) 2014-2023 wolfSSL Inc.
- License: GNU General Public License version 3 (GPLv3)
- Notes: wolfSSH is licensed under GPLv3, providing perfect license alignment with this application's GPLv3 license.

5) wolfSSL 5.8.2 (GPLv3)
- Upstream: https://www.wolfssl.com/
- GitHub: https://github.com/wolfSSL/wolfssl
- Bundled location: thirdparty/wolfssl-5.8.2/
- License file: thirdparty/wolfssl-5.8.2/COPYING
- Copyright (C) 2006-2023 wolfSSL Inc.
- License: GNU General Public License version 3 (GPLv3)
- Notes: wolfSSL is licensed under GPLv3, providing perfect license alignment with this application's GPLv3 license. wolfSSL is FIPS 140-2 Level 1 validated.

6) ESP-IDF sockets/lwIP (Apache-2.0)
- Upstream: https://github.com/espressif/esp-idf
- License: Apache License 2.0
- Notes: Provided by the base firmware; compatibility with GPLv3 is established. Retain Apache-2.0 notices in source and documentation.

Attribution in UI
- TODO: Where practical, an About or Help screen may include: "Terminal emulation by libvterm; SSH connectivity by wolfSSH; Cryptography by wolfSSL; Rendering via SDL3; Font: Leggie 9x18 by Wiktor Kerr (CC BY 4.0)."

Contact
For questions about licensing of this application, contact the author listed in manifest.json.
