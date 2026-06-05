/**
 * Tiny integer-expression evaluator — supports decimal (`123`), hex (`0x7f`),
 * binary (`0b1010`) literals and the operators `+ - * / % & | ^ << >>` plus
 * unary minus and parentheses. Returns a BigInt.
 *
 * Used by the Data Converter and Reverse Calculator dialogs (Qt parity). Not
 * a general-purpose expression engine — no floats, no functions, no variables,
 * and it intentionally throws on anything it doesn't recognise so the UI can
 * report a clean error.
 */

type Token =
  | { kind: 'num'; value: bigint }
  | { kind: 'op'; op: string }
  | { kind: 'lparen' }
  | { kind: 'rparen' };

function tokenize(src: string): Token[] {
  const out: Token[] = [];
  let i = 0;
  const s = src.trim();
  while (i < s.length) {
    const c = s[i]!;
    if (c === ' ' || c === '\t') {
      i += 1;
      continue;
    }
    if (c === '(') {
      out.push({ kind: 'lparen' });
      i += 1;
      continue;
    }
    if (c === ')') {
      out.push({ kind: 'rparen' });
      i += 1;
      continue;
    }
    if (c === '+' || c === '-' || c === '*' || c === '/' || c === '%' || c === '&' || c === '|' || c === '^') {
      out.push({ kind: 'op', op: c });
      i += 1;
      continue;
    }
    if (c === '<' && s[i + 1] === '<') {
      out.push({ kind: 'op', op: '<<' });
      i += 2;
      continue;
    }
    if (c === '>' && s[i + 1] === '>') {
      out.push({ kind: 'op', op: '>>' });
      i += 2;
      continue;
    }
    if (/[0-9]/.test(c)) {
      // Literal: decimal, 0x.., 0b..
      let j = i;
      if (c === '0' && (s[i + 1] === 'x' || s[i + 1] === 'X')) {
        j = i + 2;
        while (j < s.length && /[0-9a-fA-F]/.test(s[j]!)) j += 1;
        out.push({ kind: 'num', value: BigInt('0x' + s.slice(i + 2, j)) });
      } else if (c === '0' && (s[i + 1] === 'b' || s[i + 1] === 'B')) {
        j = i + 2;
        while (j < s.length && /[01]/.test(s[j]!)) j += 1;
        out.push({ kind: 'num', value: BigInt('0b' + s.slice(i + 2, j)) });
      } else {
        while (j < s.length && /[0-9]/.test(s[j]!)) j += 1;
        out.push({ kind: 'num', value: BigInt(s.slice(i, j)) });
      }
      i = j;
      continue;
    }
    throw new Error(`Unexpected character '${c}'`);
  }
  return out;
}

// Pratt-style recursive descent. Higher precedence binds tighter; matches
// C-family operator precedence closely enough for these dialogs.
const PRECEDENCE: Readonly<Record<string, number>> = {
  '|': 1,
  '^': 2,
  '&': 3,
  '<<': 4,
  '>>': 4,
  '+': 5,
  '-': 5,
  '*': 6,
  '/': 6,
  '%': 6,
};

function evalRPN(tokens: Token[]): bigint {
  // Shunting-yard to RPN.
  const output: Token[] = [];
  const ops: Token[] = [];
  for (const t of tokens) {
    if (t.kind === 'num') {
      output.push(t);
    } else if (t.kind === 'op') {
      while (ops.length > 0) {
        const top = ops[ops.length - 1]!;
        if (top.kind === 'op' && PRECEDENCE[top.op]! >= PRECEDENCE[t.op]!) {
          output.push(ops.pop()!);
        } else {
          break;
        }
      }
      ops.push(t);
    } else if (t.kind === 'lparen') {
      ops.push(t);
    } else if (t.kind === 'rparen') {
      while (ops.length > 0 && ops[ops.length - 1]!.kind !== 'lparen') {
        output.push(ops.pop()!);
      }
      if (ops.length === 0) throw new Error('Mismatched parenthesis');
      ops.pop();
    }
  }
  while (ops.length > 0) {
    const top = ops.pop()!;
    if (top.kind === 'lparen' || top.kind === 'rparen') {
      throw new Error('Mismatched parenthesis');
    }
    output.push(top);
  }

  const stack: bigint[] = [];
  for (const t of output) {
    if (t.kind === 'num') {
      stack.push(t.value);
      continue;
    }
    if (t.kind !== 'op') continue;
    const b = stack.pop();
    const a = stack.pop();
    if (a == null || b == null) throw new Error('Malformed expression');
    let r: bigint;
    switch (t.op) {
      case '+':
        r = a + b;
        break;
      case '-':
        r = a - b;
        break;
      case '*':
        r = a * b;
        break;
      case '/':
        if (b === 0n) throw new Error('Division by zero');
        r = a / b;
        break;
      case '%':
        if (b === 0n) throw new Error('Modulo by zero');
        r = a % b;
        break;
      case '&':
        r = a & b;
        break;
      case '|':
        r = a | b;
        break;
      case '^':
        r = a ^ b;
        break;
      case '<<':
        r = a << b;
        break;
      case '>>':
        r = a >> b;
        break;
      default:
        throw new Error(`Unknown operator: ${t.op}`);
    }
    stack.push(r);
  }
  if (stack.length !== 1) throw new Error('Malformed expression');
  return stack[0]!;
}

function rewriteUnaryMinus(tokens: Token[]): Token[] {
  // `-x` → `(0 - x)` style: replace a leading `-` (or one right after `(` or
  // an operator) with a `0` then `-` op so RPN treats it as subtraction.
  const out: Token[] = [];
  for (let i = 0; i < tokens.length; i += 1) {
    const t = tokens[i]!;
    if (
      t.kind === 'op' &&
      t.op === '-' &&
      (i === 0 ||
        tokens[i - 1]!.kind === 'op' ||
        tokens[i - 1]!.kind === 'lparen')
    ) {
      out.push({ kind: 'num', value: 0n });
    }
    out.push(t);
  }
  return out;
}

export function evalIntExpression(src: string): bigint {
  return evalRPN(rewriteUnaryMinus(tokenize(src)));
}
