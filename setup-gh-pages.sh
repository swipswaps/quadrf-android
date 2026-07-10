#!/bin/bash
# setup-gh-pages.sh – Create gh-pages branch, enable Pages, wait, and check deploy workflow

set -e

REPO="swipswaps/quadrf-android"
USERNAME="swipswaps"
PAGES_URL="https://${USERNAME}.github.io/quadrf-android/"

echo "🔐 Checking GitHub authentication..."
if ! gh auth status &>/dev/null; then
  echo "❌ Not logged in to GitHub CLI. Please run 'gh auth login' first."
  exit 1
fi

echo "📡 Checking if gh-pages branch exists remotely..."
if git ls-remote --exit-code origin gh-pages &>/dev/null; then
  echo "✅ gh-pages branch already exists."
else
  echo "⚙️  gh-pages branch does not exist. Creating it..."
  CURRENT_BRANCH=$(git branch --show-current)
  git checkout --orphan gh-pages
  git rm -rf .
  git commit --allow-empty -m "Initial gh-pages branch"
  git push origin gh-pages
  git checkout "$CURRENT_BRANCH"
  echo "✅ gh-pages branch created and pushed."
fi

echo "📡 Checking GitHub Pages status for $REPO..."
if gh api repos/$REPO/pages &>/dev/null; then
  echo "✅ GitHub Pages is already enabled."
else
  echo "⚙️  GitHub Pages not enabled. Enabling via API..."
  echo '{"build_type":"legacy","source":{"branch":"gh-pages","path":"/"}}' | \
    gh api repos/$REPO/pages -X POST --input -
  echo "✅ Pages enabled. First build may take a minute."
fi

# Check the latest deploy workflow status
echo "📊 Checking latest Deploy React App to GitHub Pages workflow..."
# Get the run ID, status, conclusion safely
RUN_INFO=$(gh run list --workflow="Deploy React App to GitHub Pages" --limit 1 --json databaseId,status,conclusion 2>/dev/null || echo "[]")
if [ "$RUN_INFO" = "[]" ]; then
  echo "⚠️  No deploy workflow runs found. It may not have triggered yet."
  echo "   Triggering it now..."
  gh workflow run deploy.yml
  echo "   Workflow triggered. It will build and deploy the site."
else
  # Extract values using jq if available, else fallback to grep/sed
  if command -v jq &>/dev/null; then
    ID=$(echo "$RUN_INFO" | jq -r '.[0].databaseId')
    STATUS=$(echo "$RUN_INFO" | jq -r '.[0].status')
    CONCLUSION=$(echo "$RUN_INFO" | jq -r '.[0].conclusion')
  else
    # Fallback: extract using grep and sed (less reliable)
    ID=$(echo "$RUN_INFO" | grep -o '"databaseId":[0-9]*' | head -1 | sed 's/.*://')
    STATUS=$(echo "$RUN_INFO" | grep -o '"status":"[^"]*"' | head -1 | sed 's/.*"status":"\([^"]*\)".*/\1/')
    CONCLUSION=$(echo "$RUN_INFO" | grep -o '"conclusion":"[^"]*"' | head -1 | sed 's/.*"conclusion":"\([^"]*\)".*/\1/')
  fi
  if [ -n "$ID" ] && [ "$ID" != "null" ]; then
    echo "   Run ID: $ID, Status: $STATUS, Conclusion: $CONCLUSION"
    if [ "$STATUS" = "completed" ] && [ "$CONCLUSION" = "success" ]; then
      echo "✅ Latest deploy workflow succeeded."
    elif [ "$STATUS" = "completed" ] && [ "$CONCLUSION" = "failure" ]; then
      echo "❌ Latest deploy workflow failed. View logs:"
      echo "   gh run view $ID --log"
      echo "   You can re-run it with: gh run rerun $ID"
      exit 1
    elif [ "$STATUS" = "in_progress" ] || [ "$STATUS" = "queued" ]; then
      echo "⏳ Deploy workflow is running (status: $STATUS). Waiting..."
    else
      echo "⚠️  Deploy workflow status: $STATUS / $CONCLUSION. Check manually:"
      echo "   gh run view $ID"
    fi
  else
    echo "⚠️  Could not parse workflow run info. It may not have started yet."
    echo "   You can trigger it manually with: gh workflow run deploy.yml"
  fi
fi

echo "⏳ Waiting for site to become available at $PAGES_URL"
max_attempts=60
attempt=0
while [ $attempt -lt $max_attempts ]; do
  attempt=$((attempt + 1))
  http_code=$(curl -s -o /dev/null -w "%{http_code}" "$PAGES_URL" || echo "000")
  if [ "$http_code" = "200" ]; then
    echo "✅ Site is live! (HTTP 200)"
    break
  else
    echo "   Attempt $attempt/$max_attempts: HTTP $http_code – waiting..."
    sleep 5
  fi
done

if [ $attempt -ge $max_attempts ]; then
  echo "⚠️  Timeout waiting for site. It may still be building."
  echo "   Check progress at: https://github.com/$REPO/actions"
  echo "   Or visit: $PAGES_URL"
else
  echo "🌐 Opening site in your browser..."
  if command -v xdg-open &>/dev/null; then
    xdg-open "$PAGES_URL"
  elif command -v open &>/dev/null; then
    open "$PAGES_URL"
  else
    echo "   Please open: $PAGES_URL"
  fi
fi
