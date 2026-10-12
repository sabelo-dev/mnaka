"""End-to-end check of the deployed Supabase project and its model. Run it by hand:

    python tests/integration/project_selftest.py

Needs the Supabase CLI logged in to the project's owner account, Pillow, and (for the Developer
workspace check) the MinGW gcc from the Qt tools. Every model call spends the owner's credit.

Creates two temporary confirmed users with the project's service role (fetched from the logged-in
Supabase CLI, never printed or stored), exercises the functions and tables as those users, then
deletes the users. Prints results only."""
import base64, io, json, os, secrets, subprocess, sys, tempfile, time, urllib.request, urllib.error
from PIL import Image, ImageDraw

REF = 'iadmtegtfcmjakrmqxdu'
URL = f'https://{REF}.supabase.co'
keys = json.loads(subprocess.run('npx --yes supabase@latest projects api-keys --project-ref ' + REF + ' -o json',
                                 shell=True, capture_output=True, text=True, cwd=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))).stdout)
keys = keys.get('keys', keys) if isinstance(keys, dict) else keys
SERVICE = next(k['api_key'] for k in keys if k.get('name') == 'service_role')
PUBLISHABLE = next(k['api_key'] for k in keys if k.get('type') == 'publishable')

def call(method, path, key, token=None, body=None, timeout=150):
    request = urllib.request.Request(URL + path, method=method, data=None if body is None else json.dumps(body).encode(),
        headers={'apikey': key, 'Authorization': 'Bearer ' + (token or key), 'Content-Type': 'application/json', 'Prefer': 'return=representation'})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            raw = response.read()
            return response.status, (json.loads(raw) if raw else None)
    except urllib.error.HTTPError as error:
        raw = error.read()
        try: return error.code, json.loads(raw)
        except Exception: return error.code, raw[:200].decode('utf-8', 'replace')

results = []
def check(name, ok, detail=''):
    results.append(ok)
    print(('PASS ' if ok else 'FAIL ') + name + (('  ' + str(detail)[:400]) if detail else ''))

users = []
try:
    for label in 'ab':
        email = f'mnaka-selftest-{label}-{secrets.token_hex(4)}@example.com'
        password = secrets.token_urlsafe(18)
        status, created = call('POST', '/auth/v1/admin/users', SERVICE, body={'email': email, 'password': password, 'email_confirm': True})
        if status not in (200, 201):
            check('create temporary user', False, created); raise SystemExit(1)
        status, session = call('POST', '/auth/v1/token?grant_type=password', PUBLISHABLE, body={'email': email, 'password': password})
        users.append({'id': created['id'], 'token': session.get('access_token') if isinstance(session, dict) else None})
        check(f'temporary user {label} signs in with the publishable key', status == 200 and bool(users[-1]['token']), '' if status == 200 else session)
    A, B = users[0]['token'], users[1]['token']

    # --- assistant ---
    status, probe = call('POST', '/functions/v1/assistant', PUBLISHABLE, A, {'probe': True})
    check('probe reports the configured model', status == 200 and probe.get('configured'), probe)
    MODEL = bool(probe.get('working', True))
    check('probe says whether the provider accepts the key', 'working' in probe, probe)
    started = time.time()
    status, answer = call('POST', '/functions/v1/assistant', PUBLISHABLE, A, {'text': 'Reply with exactly the word PINEAPPLE and nothing else.'})
    check('assistant answers through the real model', status == 200 and 'PINEAPPLE' in str(answer.get('text', '')).upper(), f"{round(time.time() - started, 1)}s {answer}")
    MODEL = MODEL and status == 200
    if MODEL: print('     model:', answer.get('model'), '| provider:', answer.get('provider'))
    else: print('     MODEL UNAVAILABLE: model-dependent checks below are skipped, storage checks continue')

    if not MODEL:
        # No model: still prove that history and memory are stored privately and in order.
        status, memory = call('POST', '/rest/v1/mnaka_memories', PUBLISHABLE, A, {'content': 'My dog is called Zolani.'})
        check('memory saved', status == 201, memory)
        now = time.time()
        stamp = lambda offset: time.strftime('%Y-%m-%dT%H:%M:%S', time.gmtime(now)) + f'.{offset:03d}Z'
        status, saved = call('POST', '/rest/v1/mnaka_messages', PUBLISHABLE, A, [{'role': 'user', 'content': 'hello', 'created_at': stamp(1)}, {'role': 'assistant', 'content': 'hi', 'created_at': stamp(2)}])
        check('history rows saved', status == 201, saved)
        status, history = call('GET', '/rest/v1/mnaka_messages?select=role&order=created_at.asc', PUBLISHABLE, A)
        check('history comes back in order', status == 200 and [r['role'] for r in history] == ['user', 'assistant'], history)
        status, edited = call('PATCH', '/rest/v1/mnaka_messages?id=not.is.null', PUBLISHABLE, A, {'content': 'rewritten'})
        check('history cannot be rewritten', status in (401, 403), status)
        status, other = call('GET', '/rest/v1/mnaka_messages?select=role', PUBLISHABLE, B)
        check('another account cannot see that history', status == 200 and other == [], other)
        status, other = call('GET', '/rest/v1/mnaka_memories?select=content', PUBLISHABLE, B)
        check('another account cannot see that memory', status == 200 and other == [], other)
    else:
        # --- memory reaches the model, history is saved in order and private ---
        status, memory = call('POST', '/rest/v1/mnaka_memories', PUBLISHABLE, A, {'content': 'My dog is called Zolani.'})
        check('memory saved', status == 201, memory)
        status, answer = call('POST', '/functions/v1/assistant', PUBLISHABLE, A, {'text': "What is my dog's name? One word."})
        check('a saved memory reaches the model', status == 200 and 'zolani' in str(answer.get('text', '')).lower(), answer)
        status, answer = call('POST', '/functions/v1/assistant', PUBLISHABLE, A, {'text': 'What was the single word I asked you to reply with at the start of this conversation?'})
        check('earlier turns are remembered', status == 200 and 'pineapple' in str(answer.get('text', '')).lower(), answer)
        status, history = call('GET', '/rest/v1/mnaka_messages?select=role,content&order=created_at.asc', PUBLISHABLE, A)
        roles = [row['role'] for row in history] if status == 200 else []
        check('history is saved in order (user, assistant, ...)', roles == ['user', 'assistant'] * 3, roles)
        status, other = call('GET', '/rest/v1/mnaka_messages?select=role', PUBLISHABLE, B)
        check("another account cannot see that history", status == 200 and other == [], other)
        status, other = call('GET', '/rest/v1/mnaka_memories?select=content', PUBLISHABLE, B)
        check("another account cannot see that memory", status == 200 and other == [], other)

        # --- vision ---
        picture = Image.new('RGB', (480, 320), 'white')
        draw = ImageDraw.Draw(picture)
        draw.ellipse((60, 60, 260, 260), fill=(220, 30, 30))
        draw.rectangle((300, 100, 440, 240), fill=(30, 60, 220))
        buffer = io.BytesIO(); picture.save(buffer, 'JPEG', quality=85)
        data_url = 'data:image/jpeg;base64,' + base64.b64encode(buffer.getvalue()).decode()
        status, answer = call('POST', '/functions/v1/assistant', PUBLISHABLE, A, {'text': 'What two shapes are in this image and what colour is each? Answer briefly.', 'image': data_url})
        text = str(answer.get('text', '')).lower() if isinstance(answer, dict) else ''
        check('vision: the model describes a real image', status == 200 and 'red' in text and 'blue' in text and ('circle' in text or 'round' in text) and ('square' in text or 'rectangle' in text), answer)
        status, history = call('GET', '/rest/v1/mnaka_messages?select=content&order=created_at.desc&limit=2', PUBLISHABLE, A)
        check('vision: the image itself is not stored', status == 200 and all('base64' not in row['content'] for row in history) and any('[image attached]' in row['content'] for row in history))

        # --- developer workspace: the proposal must be usable and the result must really compile and run ---
        main_c = '#include <stdio.h>\n\nint main(void)\n{\n    printf("total: %d\\n", 0);\n    return 0;\n}\n'
        util_h = '#ifndef UTIL_H\n#define UTIL_H\n\n#endif\n'
        started = time.time()
        status, proposal = call('POST', '/functions/v1/developer', PUBLISHABLE, A, {
            'request': 'Add a function int sum_to(int n) that returns 1+2+...+n. Declare it in util.h, define it in a new file util.c, and make main print "total: 5050" by calling sum_to(100).',
            'files': [{'path': 'main.c', 'content': main_c}, {'path': 'util.h', 'content': util_h}]}, timeout=200)
        check('developer: the model returns a change set', status == 200 and isinstance(proposal.get('files'), list) and len(proposal['files']) > 0, f"{round(time.time() - started, 1)}s {str(proposal)[:300]}")
        if status == 200:
            files = {entry['path']: entry['content'] for entry in proposal['files']}
            print('     summary:', proposal.get('summary', '')[:200]); print('     files:', sorted(files))
            check('developer: file names are plain and expected', set(files) <= {'main.c', 'util.h', 'util.c'} and 'util.c' in files, sorted(files))
            merged = {'main.c': main_c, 'util.h': util_h, **files}
            with tempfile.TemporaryDirectory() as folder:
                for name, content in merged.items():
                    open(os.path.join(folder, name), 'w', newline='\n').write(content)
                gcc = r'C:\Qt\Tools\mingw1310_64\bin\gcc.exe'
                env = dict(os.environ, PATH=r'C:\Qt\Tools\mingw1310_64\bin;' + os.environ['PATH'])
                build = subprocess.run([gcc, '-Wall', '-Werror', '-o', os.path.join(folder, 'app.exe'), os.path.join(folder, 'main.c'), os.path.join(folder, 'util.c')], capture_output=True, text=True, env=env)
                check('developer: the proposed code compiles with -Wall -Werror', build.returncode == 0, build.stderr[:300])
                if build.returncode == 0:
                    run = subprocess.run([os.path.join(folder, 'app.exe')], capture_output=True, text=True, env=env)
                    check('developer: the program prints total: 5050', run.stdout.strip() == 'total: 5050', run.stdout[:100])
        status, refused = call('POST', '/functions/v1/developer', PUBLISHABLE, A, {'request': 'x', 'files': [{'path': '../a.txt', 'content': '1'}]})
        check('developer: unsafe file names are refused before any model call', status == 400, refused)


    # --- skills and rules: storage and isolation on the real database ---
    manifest = {'id': 'selftest.skill', 'version': '1.0.0', 'artifact_hash': 'sha256:' + secrets.token_hex(8)}
    status, skill = call('POST', '/rest/v1/mnaka_skills', PUBLISHABLE, A, {'skill_id': manifest['id'], 'version': manifest['version'], 'artifact_hash': manifest['artifact_hash'], 'manifest': manifest})
    check('skills: a manifest is saved', status == 201, skill)
    status, duplicate = call('POST', '/rest/v1/mnaka_skills', PUBLISHABLE, A, {'skill_id': manifest['id'], 'version': manifest['version'], 'artifact_hash': manifest['artifact_hash'], 'manifest': manifest})
    check('skills: the same manifest twice is refused (409)', status == 409, status)
    status, rule = call('POST', '/rest/v1/mnaka_rules', PUBLISHABLE, A, {'title': 'Check the build', 'schedule': 'Weekdays 08:00'})
    check('rules: a rule is saved, switched off', status == 201 and rule[0]['enabled'] is False, rule)
    rule_id = rule[0]['id'] if status == 201 else ''
    status, toggled = call('PATCH', f'/rest/v1/mnaka_rules?id=eq.{rule_id}', PUBLISHABLE, A, {'enabled': True})
    check('rules: the owner can switch it on', status == 200 and toggled and toggled[0]['enabled'] is True, toggled)
    status, edited = call('PATCH', f'/rest/v1/mnaka_rules?id=eq.{rule_id}', PUBLISHABLE, A, {'title': 'changed'})
    check('rules: the title cannot be rewritten', status in (401, 403), status)
    status, stolen = call('PATCH', f'/rest/v1/mnaka_rules?id=eq.{rule_id}', PUBLISHABLE, B, {'enabled': False})
    check('rules: another account cannot change it', status == 200 and stolen == [], stolen)
    status, forged = call('POST', '/rest/v1/mnaka_memories', PUBLISHABLE, B, {'user_id': users[0]['id'], 'content': 'planted'})
    check('another account cannot plant data in this one', status in (401, 403), status)
    status, anonymous = call('GET', '/rest/v1/mnaka_rules?select=id', PUBLISHABLE)
    check('signed-out visitors cannot read rules', status in (401, 403), status)
    status, _ = call('DELETE', '/rest/v1/mnaka_messages?id=not.is.null', PUBLISHABLE, A)
    status2, left = call('GET', '/rest/v1/mnaka_messages?select=id', PUBLISHABLE, A)
    check('clearing the conversation deletes it', status in (200, 204) and left == [], left)
finally:
    for user in users:
        status, _ = call('DELETE', f"/auth/v1/admin/users/{user['id']}", SERVICE)
        print('cleanup: temporary user deleted' if status in (200, 204) else f'cleanup FAILED for {user["id"]} ({status})')
print(f'{sum(results)} of {len(results)} checks passed')
sys.exit(0 if all(results) else 1)
