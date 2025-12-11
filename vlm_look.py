#!/usr/bin/env python3
"""
vlm_look.py - Visual Language Model Screen Analyzer for C64 Development

Uses a local VLM (via Ollama) to analyze VICE screenshots and provide
detailed descriptions for AI coding agents.

This gives the AI agent TRUE vision of what's displayed on the C64 screen.

**IMPORTANT FOR AI AGENTS**: 
- ALWAYS wait for the VLM response, even if it takes 30-60 seconds
- NEVER skip this step or tell the user it's slow
- This is the fundamental feedback mechanism of the toolchain

Requirements:
    - Ollama running with a VLM model (qwen2-vl, qwen3-vl, llava, etc.)
    - VICE running with -remotemonitor
    - ollama Python library: pip install ollama

Usage:
    python3 vlm_look.py                    # Analyze current screen
    python3 vlm_look.py --json             # Structured JSON output
    python3 vlm_look.py --existing img.png # Analyze existing image
    python3 vlm_look.py --compare a.png --with b.png  # Compare two screenshots
    python3 vlm_look.py --prompt "..."     # Custom analysis prompt
    python3 vlm_look.py --motion 3         # Multi-frame motion analysis

Author: C64AIToolChain Project
Date: December 2024
"""

import socket
import time
import sys
import os
import json
import argparse
from pathlib import Path
from datetime import datetime

try:
    from ollama import Client
    HAS_OLLAMA = True
except ImportError:
    HAS_OLLAMA = False

try:
    from PIL import Image
    import io
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


# Configuration
OLLAMA_HOST = os.environ.get('OLLAMA_HOST', 'http://192.168.1.62:11434')
DEFAULT_MODEL = os.environ.get('OLLAMA_MODEL', 'qwen3-vl')
VICE_PORT = 6510

# History file for context tracking
HISTORY_FILE = '/tmp/vlm_look_history.json'
MAX_HISTORY = 10

# =============================================================================
# PROMPTS - General C64 Screen Analysis
# =============================================================================

DEFAULT_PROMPT = """Analyze this Commodore 64 screen. Describe in detail:

1. SCREEN LAYOUT
   - What type of content is displayed? (game, application, text, graphics)
   - What is the general color scheme? (background color, main colors used)
   - Is there a border? What color?

2. TEXT CONTENT
   - List any text visible on screen (exact text if readable)
   - Where is text positioned? (top, bottom, center)
   - Any score, status, or info displays?

3. GRAPHICS & SPRITES
   - Are there sprites visible? How many?
   - Describe each sprite: position (top/center/bottom, left/center/right), color, shape
   - Are sprites overlapping or separate?
   - Character graphics or bitmap mode?

4. SCREEN REGIONS
   - Describe what's in each area (top, middle, bottom)
   - Any distinct zones or panels?

5. CURRENT STATE
   - What state is the program in? (title screen, running, paused, input wait, etc.)
   - Is there animation happening or is it static?

6. VISUAL ISSUES
   - Any glitches, corruption, or unexpected elements?
   - Missing graphics, flickering, or alignment problems?
   - Colors that seem wrong?

Be precise with positions: use "top-left", "center", "bottom-right", etc.
For sprites, describe approximate screen coordinates if possible."""

# Optional game-specific prompts (can be enabled with --game flag)
GAME_PROMPTS = {
    'snake': """Analyze this Snake game screenshot for the C64. Focus on:
1. SNAKE: Where is the snake head? What direction is it moving? How long?
2. FOOD: Where is the apple/food relative to snake?
3. WALLS: Are walls visible on all 4 sides?
4. OBSTACLES: Any obstacles? Where?
5. SCORE: What score is displayed?
6. DANGER: Is snake about to hit wall, obstacle, or itself?
7. STATE: Playing, paused, game over, or demo?
8. BUGS: Any visual glitches or rendering issues?""",

    'pacman': """Analyze this Pac-Man game screenshot for the C64. Focus on:
1. PAC-MAN: Where is Pac-Man? What direction is it facing?
2. GHOSTS: How many ghosts? Colors? Positions relative to Pac-Man?
3. MAZE: Is maze structure visible? Dots? Power pellets?
4. SCORE/LIVES: Score displayed? Lives remaining?
5. DANGER: Is Pac-Man near a ghost?
6. STATE: Playing, paused, game over, title screen?
7. BUGS: Any visual issues?""",

    'tetris': """Analyze this Tetris game screenshot for the C64. Focus on:
1. CURRENT PIECE: What tetromino? Position? Rotation?
2. NEXT PIECE: Preview visible? What piece?
3. PLAYFIELD: How filled? Any complete lines?
4. SCORE/LEVEL: Score? Level? Lines cleared?
5. DANGER: Is stack near top?
6. STATE: Playing, paused, game over?
7. BUGS: Any visual issues?""",

    'generic': None  # Use DEFAULT_PROMPT
}

# JSON output prompt - instructs VLM to return structured data
JSON_PROMPT_TEMPLATE = """Analyze this Commodore 64 screen and return a JSON object with this structure:
{{
    "screen_type": "game|application|text|graphics|title|menu",
    "state": "running|paused|waiting|game_over|title_screen|demo",
    "text_content": ["<any text visible on screen>"],
    "sprites": [
        {{"color": "<color>", "position": {{"x": "<left|center|right>", "y": "<top|middle|bottom>"}}, "description": "<what it looks like>"}}
    ],
    "background": {{
        "color": "<main background color>",
        "border_color": "<border color>"
    }},
    "score_display": "<score value or null>",
    "visual_issues": ["<list any glitches, missing elements, or problems>"],
    "additional_observations": "<anything else noteworthy>"
}}

Return ONLY valid JSON, no markdown formatting or explanation."""

COMPARE_PROMPT = """Compare these two Commodore 64 screenshots (before and after).
Describe what changed between them:
1. Did any sprites move? From where to where?
2. Did any text or numbers change?
3. Did the screen layout change?
4. Did the program state change?
5. Any new visual elements or issues?

Be specific about the differences."""



def take_vice_screenshot(output_path: str, timeout: float = 3.0) -> bool:
    """Take a screenshot from VICE via remote monitor."""
    output_path = os.path.abspath(output_path)
    command = f'screenshot "{output_path}" 2\n'
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(('localhost', VICE_PORT))
        sock.sendall(command.encode())
        time.sleep(0.5)
        try:
            sock.recv(1024)
        except socket.timeout:
            pass
        sock.close()
        time.sleep(0.3)
        return os.path.exists(output_path)
    except socket.error as e:
        print(f"Error connecting to VICE: {e}")
        return False


def get_ollama_client(host: str = None) -> 'Client':
    """Create Ollama client with specified or default host."""
    if host is None:
        host = OLLAMA_HOST
    return Client(host=host)


# =============================================================================
# HISTORY TRACKING - For context continuity
# =============================================================================

def load_history() -> list:
    """Load observation history from file."""
    try:
        if os.path.exists(HISTORY_FILE):
            with open(HISTORY_FILE, 'r') as f:
                return json.load(f)
    except:
        pass
    return []


def save_history(history: list):
    """Save observation history to file."""
    try:
        # Keep only last N entries
        history = history[-MAX_HISTORY:]
        with open(HISTORY_FILE, 'w') as f:
            json.dump(history, f, indent=2)
    except Exception as e:
        print(f"Warning: Could not save history: {e}")


def add_to_history(observation: str, game: str = None, is_json: bool = False):
    """Add an observation to history."""
    history = load_history()
    entry = {
        'timestamp': datetime.now().isoformat(),
        'game': game,
        'observation': observation,
        'is_json': is_json
    }
    history.append(entry)
    save_history(history)


def get_context_summary() -> str:
    """Get a summary of recent observations for context."""
    history = load_history()
    if not history:
        return ""
    
    recent = history[-3:]  # Last 3 observations
    summary = "Recent observations:\n"
    for i, entry in enumerate(recent, 1):
        ts = entry.get('timestamp', 'unknown')[:19]
        obs = entry.get('observation', '')[:200]
        summary += f"{i}. [{ts}] {obs}...\n"
    return summary


def clear_history():
    """Clear observation history."""
    try:
        if os.path.exists(HISTORY_FILE):
            os.remove(HISTORY_FILE)
    except:
        pass


# =============================================================================
# VLM ANALYSIS FUNCTIONS
# =============================================================================

def analyze_with_ollama(image_path: str, prompt: str, 
                        model: str = None, host: str = None,
                        verbose: bool = True) -> str:
    """
    Send image to Ollama VLM for analysis using the ollama library.
    
    Args:
        image_path: Path to the image file
        prompt: The analysis prompt
        model: Ollama model name (must support vision)
        host: Ollama server URL
        verbose: Print progress messages
        
    Returns:
        The model's analysis text
    """
    if not HAS_OLLAMA:
        return "Error: ollama library required. Install with: pip install ollama"
    
    if model is None:
        model = DEFAULT_MODEL
    
    if not os.path.exists(image_path):
        return f"Error: Image not found: {image_path}"
    
    try:
        client = get_ollama_client(host)
        
        if verbose:
            print(f"Sending to {model} at {host or OLLAMA_HOST} for analysis...")
        
        # Use chat with image - ollama library handles base64 encoding
        response = client.chat(
            model=model,
            messages=[
                {
                    'role': 'user',
                    'content': prompt,
                    'images': [image_path]  # ollama lib accepts file paths directly
                }
            ]
        )
        
        return response['message']['content']
        
    except Exception as e:
        error_msg = str(e)
        if "connection" in error_msg.lower() or "refused" in error_msg.lower():
            return f"Error: Cannot connect to Ollama at {host or OLLAMA_HOST}. Is Ollama running?"
        elif "not found" in error_msg.lower():
            return f"Error: Model '{model}' not found. Try: ollama pull {model}"
        else:
            return f"Error calling Ollama: {e}"


def analyze_motion_with_ollama(image_paths: list, prompt: str,
                                model: str = "gemma3", host: str = None,
                                verbose: bool = True) -> str:
    """
    Analyze multiple images with Ollama to detect motion/changes.
    Uses Gemma 3 by default as it supports multi-image understanding.
    
    Args:
        image_paths: List of image file paths (in chronological order)
        prompt: The analysis prompt
        model: Ollama model name (gemma3 recommended for multi-image)
        host: Ollama server URL
        verbose: Print progress messages
        
    Returns:
        The model's motion analysis text
    """
    if not HAS_OLLAMA:
        return "Error: ollama library required. Install with: pip install ollama"
    
    # Verify all images exist
    for path in image_paths:
        if not os.path.exists(path):
            return f"Error: Image not found: {path}"
    
    try:
        client = get_ollama_client(host)
        
        if verbose:
            print(f"Analyzing {len(image_paths)} images with {model}...")
            print(f"Images: {', '.join([os.path.basename(p) for p in image_paths])}")
        
        # Send multiple images in one request
        response = client.chat(
            model=model,
            messages=[
                {
                    'role': 'user',
                    'content': prompt,
                    'images': image_paths  # Multiple images for motion analysis
                }
            ]
        )
        
        return response['message']['content']
        
    except Exception as e:
        error_msg = str(e)
        if "connection" in error_msg.lower() or "refused" in error_msg.lower():
            return f"Error: Cannot connect to Ollama at {host or OLLAMA_HOST}"
        elif "not found" in error_msg.lower():
            return f"Error: Model '{model}' not found. Try: ollama pull {model}"
        else:
            return f"Error calling Ollama: {e}"


def list_ollama_models(host: str = None) -> list:
    """List available Ollama models."""
    if not HAS_OLLAMA:
        return []
    
    try:
        client = get_ollama_client(host)
        response = client.list()
        # Handle both dict and object response formats
        if hasattr(response, 'models'):
            models = response.models
            return [m.model for m in models]
        else:
            return [m['name'] for m in response.get('models', [])]
    except Exception as e:
        print(f"Error listing models: {e}")
        return []


def quick_look(question: str = None, host: str = None, model: str = None) -> str:
    """
    Quick one-liner for AI agents to "see" the game.
    
    Args:
        question: Optional specific question about the screen
        host: Ollama host URL (uses env var if None)
        model: Model name (uses env var if None)
        
    Returns:
        VLM's analysis of the current screen
        
    Example:
        >>> from vlm_look import quick_look
        >>> print(quick_look())  # General analysis
        >>> print(quick_look("Is the snake about to die?"))  # Specific question
        >>> print(quick_look(host="http://192.168.1.62:11434"))  # Remote VLM
    """
    if question:
        prompt = f"{question}\n\nContext: This is a Commodore 64 game screenshot."
    else:
        prompt = None
    
    return look_with_vlm(prompt=prompt, host=host, model=model, verbose=False)


def look_with_vlm(prompt: str = None, model: str = None, 
                  image_path: str = None, host: str = None,
                  verbose: bool = True) -> str:
    """
    Main function: Capture screen and analyze with VLM.
    
    This is the key function for AI agent integration.
    Returns a detailed description of the current game screen.
    
    Args:
        prompt: Analysis prompt (uses DEFAULT_PROMPT if None)
        model: Ollama model name (uses DEFAULT_MODEL if None)
        image_path: Existing image to analyze (captures from VICE if None)
        host: Ollama host URL (uses OLLAMA_HOST if None)
        verbose: Print progress messages
        
    Returns:
        String containing the VLM's analysis
    """
    if not HAS_OLLAMA:
        return "Error: ollama library required. Install with: pip install ollama"
    
    if prompt is None:
        prompt = DEFAULT_PROMPT
    if model is None:
        model = DEFAULT_MODEL
    
    # Get or capture image
    temp_file = False
    if image_path is None:
        image_path = '/tmp/vice_vlm_screenshot.png'
        temp_file = True
        if verbose:
            print("Capturing screenshot from VICE...")
        if not take_vice_screenshot(image_path):
            return "Error: Failed to capture screenshot. Is VICE running with -remotemonitor?"
        if verbose:
            print(f"Screenshot saved: {image_path}")
    else:
        if not os.path.exists(image_path):
            return f"Error: Image not found: {image_path}"
    
    # Analyze with VLM
    result = analyze_with_ollama(image_path, prompt, model, host, verbose)
    
    # Clean up temp file
    if temp_file:
        try:
            os.remove(image_path)
        except:
            pass
    
    return result


# =============================================================================
# ADVANCED FUNCTIONS - JSON, Comparison, Game-Specific
# =============================================================================

def look_json(game: str = None, host: str = None, model: str = None,
              save_history: bool = True) -> dict:
    """
    Analyze screen and return structured JSON for programmatic use.
    
    Args:
        game: Game type for optimized analysis ('snake', 'tetris')
        host: Ollama host URL
        model: Model name
        save_history: Whether to save to history
        
    Returns:
        Dictionary with structured game state, or error dict
        
    Example:
        >>> from vlm_look import look_json
        >>> state = look_json(game='snake')
        >>> if state.get('danger_level') == 'high':
        ...     print("Snake is in danger!")
    """
    # Capture screenshot
    image_path = '/tmp/vice_vlm_json.png'
    if not take_vice_screenshot(image_path):
        return {'error': 'Failed to capture screenshot'}
    
    try:
        result = analyze_with_ollama(image_path, JSON_PROMPT_TEMPLATE, model, host, verbose=False)
        
        # Try to parse JSON from response
        # VLM might include markdown code blocks, so strip them
        result = result.strip()
        if result.startswith('```json'):
            result = result[7:]
        if result.startswith('```'):
            result = result[3:]
        if result.endswith('```'):
            result = result[:-3]
        result = result.strip()
        
        parsed = json.loads(result)
        
        # Add metadata
        parsed['_meta'] = {
            'timestamp': datetime.now().isoformat(),
            'game': game,
            'model': model or DEFAULT_MODEL
        }
        
        # Save to history if requested
        if save_history:
            add_to_history(json.dumps(parsed, indent=2), game=game, is_json=True)
        
        return parsed
        
    except json.JSONDecodeError as e:
        return {
            'error': 'Failed to parse JSON from VLM response',
            'raw_response': result,
            'parse_error': str(e)
        }
    finally:
        try:
            os.remove(image_path)
        except:
            pass


def look_game(game: str, host: str = None, model: str = None,
              save_history: bool = True) -> str:
    """
    Analyze screen using game-specific optimized prompt.
    
    Args:
        game: Game type ('snake', 'tetris', 'generic')
        host: Ollama host URL
        model: Model name
        save_history: Whether to save to history
        
    Returns:
        Detailed analysis optimized for the specific game
        
    Example:
        >>> from vlm_look import look_game
        >>> print(look_game('snake'))  # Snake-specific analysis
    """
    prompt = GAME_PROMPTS.get(game.lower(), GAME_PROMPTS['generic'])
    
    result = look_with_vlm(prompt=prompt, model=model, host=host, verbose=False)
    
    if save_history:
        add_to_history(result, game=game, is_json=False)
    
    return result


def compare_screenshots(image1: str, image2: str = None, 
                        host: str = None, model: str = None) -> str:
    """
    Compare two screenshots to detect changes.
    
    If image2 is None, captures current screen and compares with image1.
    
    Args:
        image1: Path to first (before) image
        image2: Path to second (after) image, or None to capture current
        host: Ollama host URL
        model: Model name
        
    Returns:
        Description of changes between the two images
        
    Example:
        >>> # Save a screenshot, make changes, then compare
        >>> take_vice_screenshot('/tmp/before.png')
        >>> # ... game runs ...
        >>> changes = compare_screenshots('/tmp/before.png')
        >>> print(changes)
    """
    if not os.path.exists(image1):
        return f"Error: Image not found: {image1}"
    
    # Capture current if image2 not provided
    temp_file = False
    if image2 is None:
        image2 = '/tmp/vice_compare_after.png'
        temp_file = True
        if not take_vice_screenshot(image2):
            return "Error: Failed to capture current screenshot"
    elif not os.path.exists(image2):
        return f"Error: Image not found: {image2}"
    
    if not HAS_OLLAMA:
        return "Error: ollama library required"
    
    try:
        client = get_ollama_client(host)
        model = model or DEFAULT_MODEL
        
        # Send both images for comparison
        response = client.chat(
            model=model,
            messages=[
                {
                    'role': 'user',
                    'content': COMPARE_PROMPT,
                    'images': [image1, image2]
                }
            ]
        )
        
        return response['message']['content']
        
    except Exception as e:
        return f"Error comparing images: {e}"
    finally:
        if temp_file:
            try:
                os.remove(image2)
            except:
                pass


def watch_game(interval: float = 2.0, count: int = 5, game: str = None,
               host: str = None, model: str = None) -> list:
    """
    Watch the game over time, capturing multiple observations.
    
    Args:
        interval: Seconds between captures
        count: Number of observations to make
        game: Game type for optimized prompts
        host: Ollama host URL
        model: Model name
        
    Returns:
        List of observations with timestamps
        
    Example:
        >>> observations = watch_game(interval=1.0, count=3, game='snake')
        >>> for obs in observations:
        ...     print(f"[{obs['time']}] {obs['summary']}")
    """
    observations = []
    prompt = GAME_PROMPTS.get(game, GAME_PROMPTS['generic']) if game else DEFAULT_PROMPT
    
    for i in range(count):
        print(f"Observation {i+1}/{count}...")
        
        result = look_with_vlm(prompt=prompt, model=model, host=host, verbose=False)
        
        observations.append({
            'index': i + 1,
            'time': datetime.now().isoformat(),
            'observation': result
        })
        
        add_to_history(result, game=game)
        
        if i < count - 1:
            time.sleep(interval)
    
    return observations


# Motion analysis prompt for multi-image understanding
MOTION_PROMPT = """You are analyzing a sequence of {count} game screenshots taken {interval} seconds apart.
These images show a Commodore 64 game in motion.

Analyze the sequence and describe:
1. SPRITE MOVEMENT: Which sprites are moving? In what direction? How fast?
2. ANIMATION: Are there any animated elements (blinking, cycling colors, etc.)?
3. GAME PROGRESS: What changed between frames? Score? Lives? Level?
4. PLAYER ACTIONS: What is the player (or AI) doing?
5. GAME EVENTS: Any collisions, pickups, deaths, or other events?
6. PATTERNS: Any repetitive movement patterns?

Be specific about what's moving and how. Use terms like "moving right", "stationary", "oscillating", etc."""


def analyze_motion(interval: float = 0.5, count: int = 3, game: str = None,
                   host: str = None, model: str = "gemma3") -> str:
    """
    Capture multiple screenshots and analyze motion using multi-image VLM.
    Uses Gemma 3 by default for multi-image understanding.
    
    Args:
        interval: Seconds between captures
        count: Number of frames to capture (2-5 recommended)
        game: Game type for context
        host: Ollama host URL
        model: Model name (gemma3 recommended)
        
    Returns:
        Motion analysis description
        
    Example:
        >>> analysis = analyze_motion(interval=0.5, count=3, game='pacman')
        >>> print(analysis)
    """
    import tempfile
    
    print(f"Capturing {count} frames at {interval}s intervals...")
    
    # Capture multiple screenshots
    image_paths = []
    for i in range(count):
        path = f"/tmp/motion_frame_{i}.png"
        if take_vice_screenshot(path):
            image_paths.append(path)
            print(f"  Frame {i+1}/{count} captured")
        else:
            print(f"  Frame {i+1}/{count} FAILED")
            
        if i < count - 1:
            time.sleep(interval)
    
    if len(image_paths) < 2:
        return "Error: Need at least 2 frames for motion analysis"
    
    # Build the prompt
    prompt = MOTION_PROMPT.format(count=len(image_paths), interval=interval)
    
    if game:
        game_context = {
            'pacman': "This is Pac-Man. Look for Pac-Man (yellow) and ghost movements.",
            'snake': "This is Snake. Look for snake head direction and body following.",
            'tetris': "This is Tetris. Look for falling piece movement and line clears."
        }
        prompt += f"\n\nGame context: {game_context.get(game, 'Unknown game type')}"
    
    # Analyze with multi-image model
    result = analyze_motion_with_ollama(image_paths, prompt, model=model, host=host)
    
    # Cleanup temp files
    for path in image_paths:
        try:
            os.remove(path)
        except:
            pass
    
    return result


# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Analyze VICE screenshots using local VLM (Ollama)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                              # Analyze current VICE screen
  %(prog)s --json                       # Structured JSON output
  %(prog)s --game snake                 # Snake-optimized analysis
  %(prog)s --game tetris                # Tetris-optimized analysis
  %(prog)s --compare before.png         # Compare before.png with current
  %(prog)s --compare a.png --with b.png # Compare two images
  %(prog)s --watch 3                    # Watch game for 3 observations
  %(prog)s --history                    # Show recent observations
  %(prog)s --model llava                # Use LLaVA model
  %(prog)s --existing game.png          # Analyze existing image
  %(prog)s --list-models                # Show available models

Environment:
  OLLAMA_HOST    Ollama server URL (default: http://192.168.1.62:11434)
  OLLAMA_MODEL   Default model (default: qwen3-vl)

For AI Agent Integration:
  from vlm_look import quick_look, look_json, look_game, compare_screenshots
  
  # Simple analysis
  print(quick_look("Is the snake in danger?"))
  
  # Structured JSON for programmatic use
  state = look_json(game='snake')
  if state.get('danger_level') == 'high':
      print("Warning: Snake in danger!")
  
  # Game-specific detailed analysis
  print(look_game('snake'))
        """
    )
    
    # Basic options
    parser.add_argument('--model', '-m', default=DEFAULT_MODEL,
                        help=f'Ollama VLM model (default: {DEFAULT_MODEL})')
    parser.add_argument('--prompt', '-p', default=None,
                        help='Custom analysis prompt')
    parser.add_argument('--existing', '-e', metavar='IMAGE',
                        help='Analyze existing image instead of capturing')
    parser.add_argument('--host', default=OLLAMA_HOST,
                        help=f'Ollama host URL (default: {OLLAMA_HOST})')
    parser.add_argument('--save', '-s', metavar='PATH',
                        help='Save screenshot to this path')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Minimal output')
    
    # New features
    parser.add_argument('--json', '-j', action='store_true',
                        help='Output structured JSON for programmatic use')
    parser.add_argument('--game', '-g', choices=['snake', 'pacman', 'tetris', 'generic'],
                        help='Use game-specific optimized prompt')
    parser.add_argument('--compare', '-c', metavar='IMAGE',
                        help='Compare IMAGE with current screen')
    parser.add_argument('--with', dest='compare_with', metavar='IMAGE2',
                        help='Second image for comparison (use with --compare)')
    parser.add_argument('--watch', '-w', type=int, metavar='N',
                        help='Watch game for N observations')
    parser.add_argument('--interval', type=float, default=2.0,
                        help='Interval between watch observations (default: 2.0)')
    parser.add_argument('--motion', type=int, metavar='FRAMES',
                        help='Capture FRAMES and analyze motion with gemma3')
    parser.add_argument('--motion-interval', type=float, default=0.5,
                        help='Interval between motion frames (default: 0.5)')
    parser.add_argument('--history', action='store_true',
                        help='Show recent observation history')
    parser.add_argument('--clear-history', action='store_true',
                        help='Clear observation history')
    parser.add_argument('--list-models', '-l', action='store_true',
                        help='List available Ollama models')
    
    args = parser.parse_args()
    
    # --- Handle utility commands first ---
    
    # List models
    if args.list_models:
        models = list_ollama_models(args.host)
        if models:
            print("Available Ollama models:")
            for m in models:
                vlm_marker = " (VLM)" if any(v in m.lower() for v in ['llava', 'qwen', 'vision', 'vl']) else ""
                print(f"  - {m}{vlm_marker}")
        else:
            print(f"Could not retrieve models from {args.host}. Is Ollama running?")
        return 0
    
    # Show history
    if args.history:
        history = load_history()
        if history:
            print(f"Recent observations ({len(history)} entries):\n")
            for entry in history[-5:]:
                ts = entry.get('timestamp', 'unknown')[:19]
                game = entry.get('game', 'unknown')
                obs = entry.get('observation', '')[:300]
                print(f"[{ts}] Game: {game}")
                print(f"{obs}...")
                print("-" * 60)
        else:
            print("No observation history.")
        return 0
    
    # Clear history
    if args.clear_history:
        clear_history()
        print("History cleared.")
        return 0
    
    # Check dependencies
    if not HAS_OLLAMA:
        print("Error: ollama library required. Install with: pip install ollama")
        return 1
    
    # --- Handle comparison mode ---
    if args.compare:
        print(f"Comparing {args.compare} with ", end="")
        if args.compare_with:
            print(f"{args.compare_with}...")
        else:
            print("current screen...")
        
        result = compare_screenshots(args.compare, args.compare_with, 
                                     args.host, args.model)
        print("\n" + "="*60)
        print("COMPARISON RESULT")
        print("="*60)
        print(result)
        print("="*60 + "\n")
        return 0
    
    # --- Handle motion analysis mode ---
    if args.motion:
        print(f"Capturing {args.motion} frames for motion analysis...")
        result = analyze_motion(
            interval=args.motion_interval,
            count=args.motion,
            game=args.game,
            host=args.host,
            model="gemma3"  # Gemma 3 for multi-image
        )
        print("\n" + "="*60)
        print("MOTION ANALYSIS (Gemma 3)")
        print("="*60)
        print(result)
        print("="*60 + "\n")
        return 0
    
    # --- Handle watch mode ---
    if args.watch:
        print(f"Watching game for {args.watch} observations (interval: {args.interval}s)...")
        observations = watch_game(
            interval=args.interval,
            count=args.watch,
            game=args.game,
            host=args.host,
            model=args.model
        )
        print("\n" + "="*60)
        print(f"WATCH RESULTS ({len(observations)} observations)")
        print("="*60)
        for obs in observations:
            print(f"\n--- Observation {obs['index']} [{obs['time'][:19]}] ---")
            print(obs['observation'][:500])
            if len(obs['observation']) > 500:
                print("...")
        print("="*60 + "\n")
        return 0
    
    # --- Handle JSON mode ---
    if args.json:
        result = look_json(game=args.game, host=args.host, model=args.model)
        print(json.dumps(result, indent=2))
        return 0 if 'error' not in result else 1
    
    # --- Handle game-specific mode ---
    if args.game and not args.prompt:
        result = look_game(args.game, host=args.host, model=args.model)
        if not args.quiet:
            print("\n" + "="*60)
            print(f"GAME ANALYSIS: {args.game.upper()}")
            print("="*60)
        print(result)
        if not args.quiet:
            print("="*60 + "\n")
        return 0
    
    # --- Default: Standard analysis ---
    
    # Determine image path
    if args.existing:
        image_path = args.existing
    elif args.save:
        image_path = args.save
        print(f"Capturing screenshot to {image_path}...")
        if not take_vice_screenshot(image_path):
            print("Failed to capture screenshot")
            return 1
    else:
        image_path = None
    
    # Determine prompt
    prompt = args.prompt or DEFAULT_PROMPT
    
    # Analyze
    result = look_with_vlm(
        prompt=prompt,
        model=args.model,
        image_path=image_path,
        host=args.host,
        verbose=not args.quiet
    )
    
    # Save to history
    add_to_history(result, game=args.game)
    
    if not args.quiet:
        print("\n" + "="*60)
        print("VLM ANALYSIS")
        print("="*60)
    print(result)
    if not args.quiet:
        print("="*60 + "\n")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
