import type { Rule } from '../core/types'
import { Modal } from './Modal'

interface RulePopupProps {
  rule: Rule
  onClose: () => void
}

// Read-only view of the grammar rule a token matched, e.g. 被…了 → bị…rồi.
export function RulePopup({ rule, onClose }: RulePopupProps) {
  return (
    <Modal
      title="Grammar rule"
      className="rule-popup"
      onClose={onClose}
      footer={(close) => (
        <button className="btn close-popup-btn" onClick={close}>
          Close
        </button>
      )}
    >
      <div className="popup-body">
        <table className="rule-table">
          <thead>
            <tr>
              <th />
              <th>Start</th>
              <th>End</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <th>Original</th>
              <td className="cn-text">{rule.originalStart}</td>
              <td className="cn-text">{rule.originalEnd}</td>
            </tr>
            <tr>
              <th>Translation</th>
              <td>{rule.translationStart}</td>
              <td>{rule.translationEnd}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </Modal>
  )
}
