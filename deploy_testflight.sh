#!/bin/bash
# PlayhouseCity - TestFlight Deployment Script
# =============================================
#
# FIRST-TIME SETUP (do these ONCE before running this script):
#
# 1. CREATE DISTRIBUTION CERTIFICATE:
#    Open Xcode > Settings > Accounts > Select your Apple ID >
#    Click "Manage Certificates" > Click "+" > "Apple Distribution"
#    This creates the certificate needed for TestFlight/App Store builds.
#
# 2. CREATE APP IN APP STORE CONNECT:
#    Go to https://appstoreconnect.apple.com
#    Click "Apps" > "+" > "New App"
#    - Platform: iOS
#    - Name: PlayhouseCity
#    - Primary Language: English (U.S.)
#    - Bundle ID: Register new bundle ID if needed, or select existing
#      (The bundle ID used is: com.playhousecity.PlayhouseCity)
#    - SKU: playhousecity (any unique string)
#
# 3. REGISTER BUNDLE ID (if not done):
#    Go to https://developer.apple.com/account/resources/identifiers
#    Click "+" > "App IDs" > "App"
#    - Description: PlayhouseCity
#    - Bundle ID: Explicit > com.playhousecity.PlayhouseCity
#    - Enable: Push Notifications, Game Center (as needed)
#
# 4. CREATE APP STORE CONNECT API KEY (for automated uploads):
#    Go to https://appstoreconnect.apple.com/access/integrations/api
#    Click "+" to generate a new key
#    - Name: PlayhouseCity Deploy
#    - Access: Admin (or Developer)
#    Save the Key ID, Issuer ID, and download the .p8 key file.
#    Place the .p8 file at: ~/.appstoreconnect/private_keys/AuthKey_<KEY_ID>.p8
#
# 5. ADD TESTERS:
#    In App Store Connect > Your App > TestFlight > Internal Testing
#    Click "+" > Add testers by email
#    They'll get an email with TestFlight install instructions.
#
# USAGE:
#   ./deploy_testflight.sh                    # Full build + upload
#   ./deploy_testflight.sh --build-only       # Build IPA without uploading
#   ./deploy_testflight.sh --upload-only      # Upload existing IPA
#
# ==============================================================================

set -e

# --- Configuration ---
PROJECT_DIR="/Users/coleparsons/PlayhouseCity"
PROJECT_FILE="$PROJECT_DIR/PlayhouseCity.uproject"
UE_ROOT="/Users/Shared/Epic Games/UE_5.7"
RUNUAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
RUNUBT="$UE_ROOT/Engine/Build/BatchFiles/RunUBT.sh"
ARCHIVE_DIR="$PROJECT_DIR/TestFlightBuild"
IPA_DIR="$ARCHIVE_DIR/IOS"

# App Store Connect API Key (set these after step 4 above)
# You can also set these as environment variables before running the script
ASC_API_KEY="${ASC_API_KEY:-REDACTED_KEY_ID}"        # Key ID from App Store Connect
ASC_API_ISSUER="${ASC_API_ISSUER:-REDACTED_ISSUER_ID}"  # Issuer ID from App Store Connect

# --- Parse arguments ---
BUILD=true
UPLOAD=true
for arg in "$@"; do
    case $arg in
        --build-only)  UPLOAD=false ;;
        --upload-only) BUILD=false ;;
        --help)
            echo "Usage: $0 [--build-only|--upload-only]"
            exit 0
            ;;
    esac
done

# --- Build ---
if [ "$BUILD" = true ]; then
    echo ""
    echo "=========================================="
    echo "  Building PlayhouseCity for TestFlight"
    echo "=========================================="
    echo ""

    # Clean previous archive
    rm -rf "$ARCHIVE_DIR"
    mkdir -p "$ARCHIVE_DIR"

    echo "[1/3] Cooking and building IOS (Shipping, Distribution)..."
    echo "       This may take several minutes..."
    echo ""

    "$RUNUAT" BuildCookRun \
        -project="$PROJECT_FILE" \
        -noP4 \
        -platform=IOS \
        -clientconfig=Shipping \
        -cook \
        -build \
        -stage \
        -pak \
        -distribution \
        -archive \
        -archivedirectory="$ARCHIVE_DIR" \
        -UTF8Output \
        2>&1 | tee "$ARCHIVE_DIR/build.log"
    BUILD_EXIT=${PIPESTATUS[0]}

    if [ $BUILD_EXIT -ne 0 ]; then
        echo ""
        echo "ERROR: Build failed (exit code $BUILD_EXIT). Check $ARCHIVE_DIR/build.log for details."
        exit 1
    fi

    # Find the .ipa file
    IPA_FILE=$(find "$ARCHIVE_DIR" -name "*.ipa" -type f 2>/dev/null | head -1)

    # If no .ipa was created by RunUAT, create one from the .app bundle
    if [ -z "$IPA_FILE" ]; then
        echo ""
        echo "[2/3] Creating IPA from app bundle..."

        APP_FILE=$(find "$ARCHIVE_DIR" -name "*.app" -type d 2>/dev/null | head -1)
        # If not in archive dir, check StagedBuilds (UE5 sometimes stages there)
        if [ -z "$APP_FILE" ]; then
            APP_FILE=$(find "$PROJECT_DIR/Saved/StagedBuilds" -name "*.app" -type d 2>/dev/null | head -1)
        fi
        if [ -z "$APP_FILE" ]; then
            echo "ERROR: No .app bundle found in $ARCHIVE_DIR or Saved/StagedBuilds"
            exit 1
        fi

        # Re-sign with distribution certificate + provisioning profile
        echo "       Re-signing with distribution certificate..."
        DIST_PROFILE="$PROJECT_DIR/Build/IOS/PlayhouseCity_Distribution.mobileprovision"
        # Use SHA-1 hash to avoid ambiguity with duplicate cert names
        DIST_IDENTITY="REDACTED_CERT_SHA1"

        # Embed distribution provisioning profile
        cp "$DIST_PROFILE" "$APP_FILE/embedded.mobileprovision"

        # Generate entitlements from the profile
        ENTITLEMENTS_FILE="/tmp/phc_entitlements_$$.plist"
        security cms -D -i "$DIST_PROFILE" 2>/dev/null | \
            plutil -extract Entitlements xml1 -o "$ENTITLEMENTS_FILE" -

        # Re-sign embedded frameworks first (Agora SDK etc.)
        if [ -d "$APP_FILE/Frameworks" ]; then
            for FW in "$APP_FILE/Frameworks"/*.framework; do
                echo "       Signing framework: $(basename $FW)"
                /usr/bin/codesign --force --sign "$DIST_IDENTITY" \
                    --timestamp=none \
                    "$FW"
            done
        fi

        # Re-sign with distribution identity
        /usr/bin/codesign --force --sign "$DIST_IDENTITY" \
            --entitlements "$ENTITLEMENTS_FILE" \
            --timestamp=none \
            --generate-entitlement-der \
            "$APP_FILE"
        rm -f "$ENTITLEMENTS_FILE"
        echo "       Re-signed successfully."

        # Create IPA (a zip of Payload/AppName.app)
        IPA_FILE="$ARCHIVE_DIR/PlayhouseCity.ipa"
        PAYLOAD_DIR=$(mktemp -d)
        mkdir -p "$PAYLOAD_DIR/Payload"
        cp -r "$APP_FILE" "$PAYLOAD_DIR/Payload/"
        cd "$PAYLOAD_DIR"
        zip -qr "$IPA_FILE" Payload
        rm -rf "$PAYLOAD_DIR"
        cd "$PROJECT_DIR"

        echo "       IPA created: $IPA_FILE"
    else
        echo ""
        echo "[2/3] IPA found: $IPA_FILE"
    fi

    echo ""
    echo "[3/3] Build complete!"
    echo "       IPA: $IPA_FILE"
    echo ""
fi

# --- Upload ---
if [ "$UPLOAD" = true ]; then
    echo ""
    echo "=========================================="
    echo "  Uploading to App Store Connect"
    echo "=========================================="
    echo ""

    # Find IPA
    IPA_FILE=$(find "$ARCHIVE_DIR" -name "*.ipa" -type f 2>/dev/null | head -1)
    if [ -z "$IPA_FILE" ]; then
        echo "ERROR: No IPA file found. Run with --build-only first or check $ARCHIVE_DIR"
        exit 1
    fi

    echo "Uploading: $IPA_FILE"
    echo ""

    if [ -n "$ASC_API_KEY" ] && [ -n "$ASC_API_ISSUER" ]; then
        # Upload using API Key (preferred, no password prompts)
        xcrun altool --upload-app \
            -f "$IPA_FILE" \
            --type ios \
            --apiKey "$ASC_API_KEY" \
            --apiIssuer "$ASC_API_ISSUER"
    else
        echo "NOTE: No API key configured. Using interactive Apple ID login."
        echo "      To automate, set ASC_API_KEY and ASC_API_ISSUER (see script header)."
        echo ""
        echo "Enter your Apple ID email:"
        read -r APPLE_ID
        echo "Enter your app-specific password (create at appleid.apple.com > Security):"
        read -rs APP_PASSWORD
        echo ""

        xcrun altool --upload-app \
            -f "$IPA_FILE" \
            --type ios \
            -u "$APPLE_ID" \
            -p "$APP_PASSWORD"
    fi

    if [ $? -eq 0 ]; then
        echo ""
        echo "=========================================="
        echo "  Upload successful!"
        echo "=========================================="
        echo ""
        echo "  Your build will appear in TestFlight within 15-30 minutes"
        echo "  after Apple's automated processing completes."
        echo ""
        echo "  Testers will be notified automatically if they have"
        echo "  'Automatic Updates' enabled in TestFlight."
        echo ""
    else
        echo ""
        echo "ERROR: Upload failed. Common issues:"
        echo "  - Missing Apple Distribution certificate"
        echo "  - App not registered in App Store Connect"
        echo "  - Invalid API key or credentials"
        echo "  - Bundle ID mismatch"
        exit 1
    fi
fi
