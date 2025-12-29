#!/bin/bash
#
# docker-build.sh - Wrapper script to build inside Docker container
#
# This is a convenience script that builds the Docker image (if needed)
# and runs the build process inside the container.
#

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

# Project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${PROJECT_ROOT}"

echo -e "${BLUE}=== Docker Build Wrapper ===${NC}"
echo ""

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo "Error: Docker is not running"
    echo "Please start Docker Desktop and try again"
    exit 1
fi

# Build Docker image if it doesn't exist
if ! docker images | grep -q "pico-spec-analyzer"; then
    echo -e "${BLUE}Building Docker image (first time only)...${NC}"
    docker-compose build
    echo ""
fi

# Run build
echo -e "${BLUE}Building project in Docker container...${NC}"
docker-compose run --rm build

echo ""
echo -e "${GREEN}✓ Docker build complete${NC}"

