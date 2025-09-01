#!/usr/bin/env python3
"""
Generate Release Notes using AI
Collects commits since last release and creates summary using Claude API
"""

import os
import sys
import subprocess
import json
import re
from typing import List, Dict
import requests

def run_git_command(cmd: List[str]) -> str:
    """Run a git command and return output"""
    try:
        result = subprocess.run(['git'] + cmd, capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Git command failed: {e}", file=sys.stderr)
        return ""

def get_last_release_tag() -> str:
    """Get the nearest tag reachable from HEAD (preferred for diff base)."""
    # Prefer the nearest/most recent reachable tag (by topology), not highest semver
    describe = run_git_command(['describe', '--tags', '--abbrev=0'])
    if describe and describe.startswith('v'):
        return describe
    # Fallback: version-sorted list of tags reachable from HEAD
    tags = run_git_command(['tag', '--sort=-version:refname', '--merged'])
    if tags:
        for tag in tags.split('\n'):
            if tag.startswith('v'):
                return tag
    return ""

def get_commits_since_tag(tag: str) -> List[Dict[str, str]]:
    """Get commits since the specified tag"""
    if not tag:
        # If no tag, get last 20 commits
        cmd = ['log', '--oneline', '-20', '--pretty=format:%H|%s|%an|%ad', '--date=short']
    else:
        cmd = ['log', f'{tag}..HEAD', '--oneline', '--pretty=format:%H|%s|%an|%ad', '--date=short']
    
    output = run_git_command(cmd)
    if not output:
        return []
    
    commits = []
    for line in output.split('\n'):
        if '|' in line:
            parts = line.split('|', 3)
            if len(parts) == 4:
                commits.append({
                    'hash': parts[0][:8],
                    'message': parts[1],
                    'author': parts[2],
                    'date': parts[3]
                })
    
    return commits

def filter_commits(commits: List[Dict[str, str]]) -> List[Dict[str, str]]:
    """Filter out automated commits and merge commits"""
    filtered = []
    skip_patterns = [
        r'Auto-increment version',
        r'Merge pull request',
        r'Merge branch',
        r'Update version\.txt',
        r'^\d+\.\d+\.\d+$',  # Version number only
    ]
    
    for commit in commits:
        message = commit['message']
        should_skip = any(re.search(pattern, message, re.IGNORECASE) for pattern in skip_patterns)
        if not should_skip and message.strip():
            filtered.append(commit)
    
    return filtered

def generate_ai_summary(commits: List[Dict[str, str]], version: str) -> str:
    """Generate AI summary using configured LLM provider (OpenAI/Anthropic)."""
    api_key = os.getenv('LLM_API_KEY')
    base_url = os.getenv('LLM_BASE_URL', '').strip()
    model = os.getenv('LLM_MODEL', 'gpt-4o-mini').strip()
    provider = os.getenv('LLM_PROVIDER', '').strip().lower()  # optional explicit provider

    if not api_key:
        print("No LLM_API_KEY found, generating basic summary", file=sys.stderr)
        return generate_basic_summary(commits, version)

    # Normalize base_url
    if not base_url:
        base_url = 'https://api.openai.com'
    elif not base_url.startswith('http'):
        base_url = f'https://{base_url}'
    base_url = base_url.rstrip('/')

    # Infer provider if not set
    if not provider:
        if 'anthropic' in base_url or model.lower().startswith('claude'):
            provider = 'anthropic'
        else:
            provider = 'openai'

    # Debug info to stderr
    print(f"LLM provider: {provider}", file=sys.stderr)
    print(f"Using API: {base_url}", file=sys.stderr)
    print(f"Using model: {model}", file=sys.stderr)

    # Prepare commits text
    commits_text = "\n".join([
        f"- {commit['hash']}: {commit['message']} (by {commit['author']})"
        for commit in commits
    ])

    prompt = f"""You are generating release notes for DPV Control version {version}, an Arduino/ESP32 project for controlling an underwater scooter (Dive Propulsion Vehicle).

Please analyze these commits and create professional release notes in Markdown format:

{commits_text}

Instructions:
- Group changes into logical categories (🚀 Features, 🐛 Bug Fixes, 🔧 Improvements, 📚 Documentation, etc.)
- Write in English
- Be concise but informative
- Focus on user-facing changes
- Ignore technical details like linting fixes unless they're important
- Use bullet points for each change
- Don't mention commit hashes or authors in the final output

If there are no significant changes, write: "Minor improvements and bug fixes."""  # Simplified heading

    try:
        if provider == 'anthropic':
            # Anthropic Messages API
            headers = {
                'x-api-key': api_key,
                'anthropic-version': '2023-06-01',
                'content-type': 'application/json',
            }
            data = {
                'model': model,
                'max_tokens': 1000,
                'messages': [
                    {
                        'role': 'user',
                        'content': prompt,
                    }
                ],
            }
            resp = requests.post(f'{base_url}/v1/messages', headers=headers, json=data, timeout=45)
            resp.raise_for_status()
            result = resp.json()
            # Extract text segments
            content_parts = []
            for block in result.get('content', []):
                if isinstance(block, dict) and block.get('type') == 'text':
                    content_parts.append(block.get('text', ''))
            content = "\n".join(content_parts).strip()
            return content if content else generate_basic_summary(commits, version)
        else:
            # OpenAI-compatible Chat Completions
            if model.startswith('openai/'):
                model_to_use = model[7:]
            else:
                model_to_use = model
            headers = {
                'Authorization': f'Bearer {api_key}',
                'Content-Type': 'application/json'
            }
            data = {
                'model': model_to_use,
                'messages': [
                    {
                        'role': 'user',
                        'content': prompt
                    }
                ],
                'max_tokens': 1000,
                'temperature': 0.7
            }
            resp = requests.post(f'{base_url}/v1/chat/completions', headers=headers, json=data, timeout=45)
            resp.raise_for_status()
            result = resp.json()
            content = result.get('choices', [{}])[0].get('message', {}).get('content', '')
            return content.strip() if content else generate_basic_summary(commits, version)
    except Exception as e:
        print(f"AI generation failed: {e}", file=sys.stderr)
        return generate_basic_summary(commits, version)

def generate_basic_summary(commits: List[Dict[str, str]], version: str) -> str:
    """Generate basic summary without AI"""
    if not commits:
        return f"# Release Notes\n\n## Version {version}\n\nMinor improvements and bug fixes."
    
    summary = f"# Release Notes\n\n## Version {version}\n\n"
    
    # Categorize commits
    features = []
    fixes = []
    improvements = []
    
    for commit in commits[:10]:  # Limit to 10 most recent
        message = commit['message'].lower()
        if any(word in message for word in ['add', 'new', 'feature', 'implement']):
            features.append(commit['message'])
        elif any(word in message for word in ['fix', 'bug', 'error', 'issue']):
            fixes.append(commit['message'])
        else:
            improvements.append(commit['message'])
    
    if features:
        summary += "🚀 **Neue Features**\n"
        for feature in features:
            summary += f"- {feature}\n"
        summary += "\n"
    
    if fixes:
        summary += "🐛 **Bugfixes**\n"
        for fix in fixes:
            summary += f"- {fix}\n"
        summary += "\n"
    
    if improvements:
        summary += "🔧 **Verbesserungen**\n"
        for improvement in improvements:
            summary += f"- {improvement}\n"
        summary += "\n"
    
    return summary.strip()

def main():
    """Main function"""
    if len(sys.argv) != 2:
        print("Usage: python generate_release_notes.py <version>", file=sys.stderr)
        sys.exit(1)
    
    version = sys.argv[1]
    
    # Get last release tag
    last_tag = get_last_release_tag()
    print(f"Last release tag: {last_tag or 'None found'}", file=sys.stderr)
    
    # Get commits since last release
    commits = get_commits_since_tag(last_tag)
    print(f"Found {len(commits)} commits since last release", file=sys.stderr)
    
    # Filter commits
    filtered_commits = filter_commits(commits)
    print(f"Filtered to {len(filtered_commits)} relevant commits", file=sys.stderr)
    
    if not filtered_commits:
        notes = f"# Release Notes\n\n## Version {version}\n\nMinor improvements and bug fixes."
    else:
        # Generate AI summary
        notes = generate_ai_summary(filtered_commits, version)
    
    # Only output the release notes to stdout (used by GitHub Actions)
    # No need to write to file - GitHub Actions captures stdout directly
    print(notes)
    
    return 0

if __name__ == '__main__':
    sys.exit(main()) 