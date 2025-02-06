# /*
#     Copyright (C) 2016-2025 zafaco GmbH
#     Copyright (C) 2019 alladin-IT GmbH

#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU Affero General Public License version 3
#     as published by the Free Software Foundation.

#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU Affero General Public License for more details.

#     You should have received a copy of the GNU Affero General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
# */

Pod::Spec.new do |s|
  s.name             = 'ias-client-cpp'
  s.version          = '2.8'
  s.summary          = 'ias-client-cpp for iOS'

  s.description      = 'ias-client-cpp for iOS'

  s.homepage         = 'https://zafaco.de'
  s.license          = { :type => 'AGPL', :file => 'LICENSE' }
  s.authors          = 'zafaco GmbH'

  s.source           = {
    :git => 'https://github.com/breitbandmessung/ias-client-cpp',
  }

  s.ios.deployment_target = '14.0'

  s.cocoapods_version = '>= 1.4.0'
  s.static_framework = true
  s.prefix_header_file = false

  s.source_files = [
    'src/**/*.{h,m,mm,cpp}',
  ]

  s.exclude_files = [
    'src/android_connector.{h,cpp}',
  ]

  s.dependency 'openssl-iosx', '~> 1.1.1w'
  s.dependency 'ias-libtool'

  s.ios.frameworks = 'MobileCoreServices', 'SystemConfiguration'

  s.library = 'c++'
  s.pod_target_xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++14',
    'GCC_PREPROCESSOR_DEFINITIONS' => 'IAS_CLIENT=1 IAS_IOS=1',
  }

  s.compiler_flags = '$(inherited) -Wreorder -Werror=reorder'
end
