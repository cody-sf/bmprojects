#!/usr/bin/env ruby
# Regenerates BMLightsWatch.xcodeproj from the sources on disk.
#
#   ruby scripts/generate-project.rb
#
# The project file is checked in so you can just open it, but it is disposable:
# add a Swift file anywhere under BMLightsWatch/ and re-run this to pick it up.

require 'xcodeproj'
require 'pathname'

ROOT = Pathname.new(__dir__).parent.expand_path
PROJECT_PATH = ROOT + 'BMLightsWatch.xcodeproj'
SOURCE_DIR = ROOT + 'BMLightsWatch'
TARGET_NAME = 'BMLightsWatch'
BUNDLE_ID = 'com.codylarocque.bmlights.watchkitapp'
DEVELOPMENT_TEAM = 'M37SRJ3YDP'
DEPLOYMENT_TARGET = '10.0'

FileUtils.rm_rf(PROJECT_PATH)
project = Xcodeproj::Project.new(PROJECT_PATH)

target = project.new_target(:application, TARGET_NAME, :watchos, DEPLOYMENT_TARGET)

# A watch-only app: no iOS companion to install it from, no phone at the show.
common_settings = {
  'ASSETCATALOG_COMPILER_APPICON_NAME' => 'AppIcon',
  'ASSETCATALOG_COMPILER_GLOBAL_ACCENT_COLOR_NAME' => 'AccentColor',
  'CODE_SIGN_STYLE' => 'Automatic',
  'CURRENT_PROJECT_VERSION' => '1',
  'DEVELOPMENT_TEAM' => DEVELOPMENT_TEAM,
  'ENABLE_PREVIEWS' => 'YES',
  'GENERATE_INFOPLIST_FILE' => 'NO',
  'INFOPLIST_FILE' => 'BMLightsWatch/Info.plist',
  'MARKETING_VERSION' => '1.0',
  'PRODUCT_BUNDLE_IDENTIFIER' => BUNDLE_ID,
  'PRODUCT_NAME' => '$(TARGET_NAME)',
  'SDKROOT' => 'watchos',
  'SKIP_INSTALL' => 'NO',
  'SUPPORTED_PLATFORMS' => 'watchos watchsimulator',
  'SWIFT_EMIT_LOC_STRINGS' => 'YES',
  'SWIFT_VERSION' => '5.0',
  'TARGETED_DEVICE_FAMILY' => '4',
  'WATCHOS_DEPLOYMENT_TARGET' => DEPLOYMENT_TARGET,
}

target.build_configurations.each do |config|
  config.build_settings.merge!(common_settings)
  if config.name == 'Debug'
    config.build_settings['SWIFT_OPTIMIZATION_LEVEL'] = '-Onone'
    config.build_settings['SWIFT_ACTIVE_COMPILATION_CONDITIONS'] = 'DEBUG'
    config.build_settings['ONLY_ACTIVE_ARCH'] = 'YES'
  else
    config.build_settings['SWIFT_OPTIMIZATION_LEVEL'] = '-O'
    config.build_settings['SWIFT_COMPILATION_MODE'] = 'wholemodule'
  end
end

project.build_configurations.each do |config|
  config.build_settings['SDKROOT'] = 'watchos'
  config.build_settings['WATCHOS_DEPLOYMENT_TARGET'] = DEPLOYMENT_TARGET
end

# Mirror the folder layout into Xcode groups so the navigator matches the disk.
root_group = project.main_group.new_group(TARGET_NAME, 'BMLightsWatch')
groups = { '.' => root_group }

group_for = lambda do |relative_dir|
  return root_group if relative_dir == '.'
  groups[relative_dir] ||= begin
    parent = group_for.call(File.dirname(relative_dir))
    parent.new_group(File.basename(relative_dir), File.basename(relative_dir))
  end
end

swift_files = Dir.glob(SOURCE_DIR + '**/*.swift').sort
raise 'no Swift sources found' if swift_files.empty?

swift_files.each do |path|
  relative = Pathname.new(path).relative_path_from(SOURCE_DIR)
  file_ref = group_for.call(File.dirname(relative.to_s)).new_reference(File.basename(path))
  target.add_file_references([file_ref])
end

# Resources.
%w[Assets.xcassets].each do |name|
  ref = root_group.new_reference(name)
  target.add_resources([ref])
end
root_group.new_reference('Info.plist')

# A shared scheme so `xcodebuild -scheme BMLightsWatch` works from a clean clone.
project.save

scheme = Xcodeproj::XCScheme.new
scheme.add_build_target(target)
scheme.set_launch_target(target)
scheme.save_as(PROJECT_PATH.to_s, TARGET_NAME, true)

puts "generated #{PROJECT_PATH.relative_path_from(ROOT.parent)}"
puts "  #{swift_files.count} Swift files, target #{TARGET_NAME} (watchOS #{DEPLOYMENT_TARGET})"
