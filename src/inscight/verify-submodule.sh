#!/bin/bash
# Script to verify SystemC submodule configuration for Zarr support
# Usage: ./verify-submodule.sh [path/to/avp64]

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

AVP64_ROOT="${1:-~/Dev/avp64}"
AVP64_ROOT=$(eval echo "$AVP64_ROOT")

echo "=========================================="
echo "SystemC Submodule Verification Script"
echo "=========================================="
echo ""

# Check if AVP64 directory exists
if [ ! -d "$AVP64_ROOT" ]; then
    echo -e "${RED}ERROR: AVP64 directory not found at: $AVP64_ROOT${NC}"
    echo "Usage: $0 [path/to/avp64]"
    exit 1
fi

echo -e "${GREEN}✓${NC} Found AVP64 directory: $AVP64_ROOT"
cd "$AVP64_ROOT"

# Check if it's a git repository
if [ ! -d ".git" ]; then
    echo -e "${RED}✗${NC} $AVP64_ROOT is not a git repository"
    exit 1
fi
echo -e "${GREEN}✓${NC} AVP64 is a git repository"

# Check for .gitmodules
echo ""
echo "Checking for git submodules..."
if [ ! -f ".gitmodules" ]; then
    echo -e "${YELLOW}⚠${NC} No .gitmodules file found"
    echo "  SystemC may not be configured as a submodule"
else
    echo -e "${GREEN}✓${NC} Found .gitmodules file"
    echo ""
    echo "Submodules in .gitmodules:"
    echo "----------------------------"
    cat .gitmodules
    echo "----------------------------"
fi

# Check for systemc submodule specifically
echo ""
echo "Checking for SystemC submodule..."
SYSTEMC_SUBMODULE=$(git config --file .gitmodules --get-regexp path | grep systemc || echo "")

if [ -z "$SYSTEMC_SUBMODULE" ]; then
    echo -e "${YELLOW}⚠${NC} SystemC not found as a submodule"
    echo ""
    echo "Searching for SystemC directories..."
    find . -maxdepth 4 -type d -name "*systemc*" -not -path "*/\.*" | while read dir; do
        echo "  Found: $dir"
        if [ -d "$dir/.git" ]; then
            cd "$dir"
            REMOTE=$(git remote -v | grep fetch | awk '{print $2}')
            echo "    Remote: $REMOTE"
            cd "$AVP64_ROOT"
        fi
    done
else
    echo -e "${GREEN}✓${NC} SystemC is configured as a submodule"
    echo ""
    echo "SystemC submodule configuration:"
    echo "----------------------------"
    git config --file .gitmodules --get-regexp 'submodule\..*systemc.*'
    echo "----------------------------"

    # Get the submodule path and URL
    SUBMODULE_PATH=$(echo "$SYSTEMC_SUBMODULE" | awk '{print $2}')
    SUBMODULE_URL=$(git config --file .gitmodules --get submodule."$SUBMODULE_PATH".url || echo "unknown")

    echo ""
    echo "SystemC submodule details:"
    echo "  Path: $SUBMODULE_PATH"
    echo "  URL:  $SUBMODULE_URL"

    # Check if it's the correct fork
    if [[ "$SUBMODULE_URL" == *"dustij/systemc"* ]]; then
        echo -e "  ${GREEN}✓${NC} Using dustij/systemc fork (Zarr-enabled)"
    else
        echo -e "  ${YELLOW}⚠${NC} NOT using dustij/systemc fork"
        echo ""
        echo "To switch to the Zarr-enabled fork:"
        echo "  git config --file .gitmodules submodule.$SUBMODULE_PATH.url git@github.com:dustij/systemc.git"
        echo "  git submodule sync"
        echo "  git submodule update --init --recursive --remote"
    fi

    # Check if submodule is initialized
    echo ""
    if [ -d "$SUBMODULE_PATH/.git" ] || [ -f "$SUBMODULE_PATH/.git" ]; then
        echo -e "${GREEN}✓${NC} Submodule is initialized"

        cd "$SUBMODULE_PATH"
        ACTUAL_REMOTE=$(git remote -v | grep origin | grep fetch | awk '{print $2}')
        CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
        CURRENT_COMMIT=$(git rev-parse --short HEAD)

        echo "  Actual remote: $ACTUAL_REMOTE"
        echo "  Current branch: $CURRENT_BRANCH"
        echo "  Current commit: $CURRENT_COMMIT"

        # Check for inscight directory
        echo ""
        if [ -d "src/inscight" ]; then
            echo -e "${GREEN}✓${NC} Found src/inscight directory"

            # Check for Zarr support files
            if [ -f "src/inscight/database_zarr.h" ] && [ -f "src/inscight/database_zarr.cpp" ]; then
                echo -e "${GREEN}✓${NC} Found Zarr database files (database_zarr.h, database_zarr.cpp)"
            else
                echo -e "${RED}✗${NC} Zarr database files not found"
                echo "  This may not be the correct SystemC fork"
            fi
        else
            echo -e "${RED}✗${NC} src/inscight directory not found"
        fi

        cd "$AVP64_ROOT"
    else
        echo -e "${RED}✗${NC} Submodule is NOT initialized"
        echo "  Run: git submodule update --init --recursive"
    fi
fi

echo ""
echo "=========================================="
echo "Verification complete"
echo "=========================================="
