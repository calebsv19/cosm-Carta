# Public Docs Policy

This policy defines what belongs in `shared/docs/` for public release.

## Keep In Public Docs
- stable architecture references
- ownership boundaries and contracts
- versioning and compatibility references
- module usage guides for external consumers

## Move To Workspace Planning Docs
Store internal-only planning and operator notes outside `shared/`.
Recommended workspace docs path:
- `CodeWork/docs/`

Examples of private-only docs:
- internal execution scratch plans
- personal/operator runbooks
- machine-local command histories
- internal migration workpads not intended for public users

## Quality Rules
- avoid machine-specific absolute paths when possible
- avoid embedding private host/user details
- keep docs concise and consumer-facing
