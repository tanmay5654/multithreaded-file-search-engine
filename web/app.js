const picker = document.querySelector('#folder-input');
const queryInput = document.querySelector('#query');
const form = document.querySelector('#search-form');
const resultsList = document.querySelector('#results');
const emptyState = document.querySelector('#empty-state');
const statusText = document.querySelector('#status-text');
const statsText = document.querySelector('#index-stats');
const folderTitle = document.querySelector('#folder-title');
const folderSubtitle = document.querySelector('#folder-subtitle');
const emptyTitle = document.querySelector('#empty-title');
const emptyDescription = document.querySelector('#empty-description');
let documents = [];
let averageLength = 0;
let documentFrequency = new Map();
let debounce;

function tokenize(text) {
  return text.toLocaleLowerCase().match(/[\p{L}\p{N}]+/gu) || [];
}

async function readFile(file) {
  try {
    if (file.size > 8 * 1024 * 1024) return null;
    const text = await file.text();
    // Skip files that are likely binary or too large for a browser-side text index.
    if (text.includes('\0')) return null;
    const terms = tokenize(text);
    const frequencies = new Map();
    for (const term of terms) frequencies.set(term, (frequencies.get(term) || 0) + 1);
    return { file, text, terms: frequencies, length: terms.length };
  } catch {
    return null;
  }
}

picker.addEventListener('change', async () => {
  const files = [...picker.files];
  if (!files.length) return;
  queryInput.disabled = true;
  resultsList.replaceChildren();
  emptyState.hidden = false;
  emptyTitle.textContent = 'Building your index…';
  emptyDescription.textContent = 'Reading selected files locally in your browser.';
  folderTitle.textContent = files[0].webkitRelativePath.split('/')[0] || 'Selected folder';
  folderSubtitle.textContent = `${files.length.toLocaleString()} files selected`;
  statusText.textContent = 'Indexing files';
  statsText.textContent = '';

  const parsed = [];
  // Keep simultaneous reads bounded so large folders do not overwhelm the browser.
  const concurrency = Math.min(8, Math.max(1, navigator.hardwareConcurrency || 4));
  for (let start = 0; start < files.length; start += concurrency) {
    const batch = await Promise.all(files.slice(start, start + concurrency).map(readFile));
    parsed.push(...batch.filter(Boolean));
    statusText.textContent = `Indexing ${Math.min(start + concurrency, files.length).toLocaleString()} of ${files.length.toLocaleString()} files`;
  }
  documents = parsed;
  averageLength = documents.reduce((sum, doc) => sum + doc.length, 0) / (documents.length || 1);
  documentFrequency = new Map();
  for (const doc of documents) {
    for (const term of doc.terms.keys())
      documentFrequency.set(term, (documentFrequency.get(term) || 0) + 1);
  }
  queryInput.disabled = documents.length === 0;
  queryInput.value = '';
  statsText.textContent = `${documents.length.toLocaleString()} indexed`;
  statusText.textContent = documents.length ? 'Ready to search' : 'No readable text files found';
  emptyTitle.textContent = documents.length ? 'Your files are ready.' : 'Nothing to index yet.';
  emptyDescription.textContent = documents.length
    ? 'Try a name, phrase, or a few keywords.'
    : 'Choose a folder with text documents to get started.';
  queryInput.focus();
});

form.addEventListener('submit', event => event.preventDefault());
queryInput.addEventListener('input', () => {
  clearTimeout(debounce);
  debounce = setTimeout(() => search(queryInput.value), 120);
});

function search(query) {
  const terms = [...new Set(tokenize(query))];
  resultsList.replaceChildren();
  if (!terms.length) {
    emptyState.hidden = false;
    emptyTitle.textContent = documents.length ? 'Your files are ready.' : 'A little less digging.';
    emptyDescription.textContent = documents.length
      ? 'Try a name, phrase, or a few keywords.'
      : 'Choose a folder and your files will be ready to search.';
    statusText.textContent = documents.length ? 'Ready to search' : 'Select a folder to begin';
    return;
  }

  const ranked = [];
  for (const doc of documents) {
    let score = 0;
    let matching = 0;
    for (const term of terms) {
      const tf = doc.terms.get(term) || 0;
      if (!tf) continue;
      matching++;
      const df = documentFrequency.get(term) || 0;
      const idf = Math.log(1 + (documents.length - df + 0.5) / (df + 0.5));
      const k1 = 1.2, b = 0.75;
      score += idf * (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * doc.length / (averageLength || 1)));
    }
    if (matching) ranked.push({ doc, score, matching });
  }
  ranked.sort((a, b) => b.score - a.score || a.doc.file.name.localeCompare(b.doc.file.name));
  emptyState.hidden = ranked.length > 0;
  if (!ranked.length) {
    emptyTitle.textContent = 'No matches this time.';
    emptyDescription.textContent = 'Try different words or choose another folder.';
  }
  statusText.textContent = `${ranked.length.toLocaleString()} result${ranked.length === 1 ? '' : 's'}`;
  statsText.textContent = 'Ranked by relevance';
  for (const [index, item] of ranked.slice(0, 100).entries()) resultsList.append(makeResult(item, terms, index));
}

function makeResult({ doc, score, matching }, terms, index) {
  const li = document.createElement('li');
  li.className = 'result';
  li.style.animationDelay = `${Math.min(index, 12) * 20}ms`;
  const extension = doc.file.name.includes('.') ? doc.file.name.split('.').pop().slice(0, 4).toUpperCase() : 'TXT';
  const icon = document.createElement('span');
  icon.className = 'file-icon';
  icon.textContent = extension;
  const main = document.createElement('div');
  main.className = 'result-main';
  const name = document.createElement('div');
  name.className = 'result-name';
  name.textContent = doc.file.name;
  const path = document.createElement('div');
  path.className = 'result-path';
  path.textContent = doc.file.webkitRelativePath || doc.file.name;
  const snippet = document.createElement('div');
  snippet.className = 'snippet';
  snippet.append(highlightedSnippet(doc.text, terms));
  main.append(name, path, snippet);
  const badge = document.createElement('span');
  badge.className = 'score';
  badge.textContent = `${matching} term${matching === 1 ? '' : 's'} · ${score.toFixed(2)}`;
  li.append(icon, main, badge);
  return li;
}

function highlightedSnippet(text, terms) {
  const lowered = text.toLocaleLowerCase();
  let at = -1;
  for (const term of terms) {
    const found = lowered.indexOf(term);
    if (found !== -1 && (at === -1 || found < at)) at = found;
  }
  const start = Math.max(0, (at === -1 ? 0 : at) - 65);
  const end = Math.min(text.length, start + 180);
  const fragment = text.slice(start, end);
  const regex = new RegExp(`(${terms.map(escapeRegex).join('|')})`, 'giu');
  const output = document.createDocumentFragment();
  if (start > 0) output.append('…');
  let cursor = 0;
  for (const match of fragment.matchAll(regex)) {
    output.append(document.createTextNode(fragment.slice(cursor, match.index)));
    const mark = document.createElement('mark');
    mark.textContent = match[0];
    output.append(mark);
    cursor = match.index + match[0].length;
  }
  output.append(document.createTextNode(fragment.slice(cursor)));
  if (end < text.length) output.append('…');
  return output;
}

function escapeRegex(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}
