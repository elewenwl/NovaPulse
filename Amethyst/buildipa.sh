#!/bin/bash
rm -rf ./build
xcodebuild clean build CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO -sdk iphoneos -configuration Release
strip build/Release-iphoneos/Amethyst.app/Amethyst
ldid -S build/Release-iphoneos/Amethyst.app/Amethyst
mkdir build/Release-iphoneos/Payload
mv build/Release-iphoneos/Amethyst.app build/Release-iphoneos/Payload
ditto -c -k --sequesterRsrc --keepParent build/Release-iphoneos/Payload Amethyst.ipa